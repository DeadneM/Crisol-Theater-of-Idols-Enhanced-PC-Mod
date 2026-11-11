#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d11.h>
#include <d3d11on12.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <Xinput.h>
#include <MinHook.h>
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "OverlayLayout.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cwchar>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <bit>
#include <mutex>
#include <string>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
using Microsoft::WRL::ComPtr;
namespace layout=dg::overlay::layout;

namespace {
HMODULE g_module=nullptr;
std::wstring g_ini,g_log;
std::mutex g_logMutex;
std::once_flag g_initOnce;
std::atomic_bool g_menuOpen{false},g_imguiReady{false},g_failed{false};
std::atomic_int g_captureHotkey{0},g_menuVK{VK_INSERT},g_consoleScan{0x29};
std::atomic_int g_worldFov{90},g_fpsFov{85};
std::atomic_bool g_worldEnabled{false},g_fpsEnabled{false};
std::atomic_bool g_captureCamera{false},g_suspendFov{false};
std::atomic_uintptr_t g_playerCamera{0};
std::atomic<float> g_nativeWorld{0.f},g_nativeFps{0.f};
std::atomic<float> g_baselineWorld{0.f},g_baselineFps{0.f};
std::atomic_uint g_cameraCalls{0},g_firstPersonCandidates{0};
std::atomic_uint g_worldApplied{0},g_fpsApplied{0},g_scopeBypass{0};
std::atomic_bool g_cameraHookReady{false};

std::atomic_int g_wheelDelta{0};
std::atomic_uint g_charWrite{0},g_charRead{0};
std::atomic_uint g_chars[64]{};

HWND g_gameHwnd=nullptr;
WNDPROC g_previousWndProc=nullptr;
ComPtr<IDXGISwapChain> g_swap;
ComPtr<IDXGISwapChain3> g_swap3;
ComPtr<ID3D12CommandQueue> g_queue;
ComPtr<ID3D11Device> g_device;
ComPtr<ID3D11DeviceContext> g_context;
ComPtr<ID3D11On12Device> g_on12;
ComPtr<ID3D11RenderTargetView> g_d3d11Rtv;
struct Frame {ComPtr<ID3D11Texture2D> wrapped;ComPtr<ID3D11RenderTargetView> rtv;};
Frame g_dx12Frames[8];
bool g_isDx12=false;

using FactorySwap=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**);
using FactoryHwnd=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**);
using FactoryComposition=HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,const DXGI_SWAP_CHAIN_DESC1*,IDXGIOutput*,IDXGISwapChain1**);
using Present=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT);
using Present1=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*);
using ResizeBuffers=HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
FactorySwap g_originalFactorySwap=nullptr;
FactoryHwnd g_originalFactoryHwnd=nullptr;
FactoryComposition g_originalFactoryComposition=nullptr;
Present g_originalPresent=nullptr;
Present1 g_originalPresent1=nullptr;
ResizeBuffers g_originalResize=nullptr;
void* g_hookedFactorySwap=nullptr,*g_hookedFactoryHwnd=nullptr,*g_hookedFactoryComposition=nullptr;
void* g_hookedPresent=nullptr,*g_hookedPresent1=nullptr,*g_hookedResize=nullptr;

void Log(const wchar_t* msg){
    if(g_log.empty())return;
    std::lock_guard lock(g_logMutex);
    HANDLE file=CreateFileW(g_log.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return;
    SYSTEMTIME t{};GetLocalTime(&t);
    wchar_t prefix[40]{};
    swprintf_s(prefix,L"[%02u:%02u:%02u] ",t.wHour,t.wMinute,t.wSecond);
    DWORD n{};
    // Log as UTF-16 with BOM only when created by this release.
    LARGE_INTEGER size{};
    if(GetFileSizeEx(file,&size)&&size.QuadPart==0){
        const WORD bom=0xFEFF;WriteFile(file,&bom,sizeof(bom),&n,nullptr);
    }
    WriteFile(file,prefix,DWORD(wcslen(prefix)*sizeof(wchar_t)),&n,nullptr);
    WriteFile(file,msg,DWORD(wcslen(msg)*sizeof(wchar_t)),&n,nullptr);
    static const wchar_t lf[]=L"\r\n";
    WriteFile(file,lf,sizeof(lf)-sizeof(wchar_t),&n,nullptr);
    CloseHandle(file);
}
void SaveSettings(){
    if(g_ini.empty())return;
    wchar_t v[30];
    swprintf_s(v,L"%d",g_menuVK.load());WritePrivateProfileStringW(L"Hotkeys",L"MenuVirtualKey",v,g_ini.c_str());
    swprintf_s(v,L"%d",g_consoleScan.load());WritePrivateProfileStringW(L"Hotkeys",L"DeveloperConsoleScanCode",v,g_ini.c_str());
    swprintf_s(v,L"%d",g_worldFov.load());WritePrivateProfileStringW(L"Camera",L"WorldFOV",v,g_ini.c_str());
    WritePrivateProfileStringW(L"Camera",L"WorldFOVEnabled",g_worldEnabled.load()?L"1":L"0",g_ini.c_str());
    WritePrivateProfileStringW(L"Camera",L"FPSFOVEnabled",g_fpsEnabled.load()?L"1":L"0",g_ini.c_str());
    swprintf_s(v,L"%d",g_fpsFov.load());WritePrivateProfileStringW(L"Camera",L"FPSFOV",v,g_ini.c_str());
}
void InitPathAndSettings(){
    wchar_t path[MAX_PATH]{};
    DWORD n=GetModuleFileNameW(g_module,path,MAX_PATH);
    if(!n||n==MAX_PATH)return;
    wchar_t* slash=wcsrchr(path,L'\\');if(!slash)return;
    slash[1]=0;std::wstring dir(path);
    g_ini=dir+L"CRToiEnhanced.ini";
    g_log=dir+L"CRToiEnhanced.log";
    g_menuVK=std::clamp(int(GetPrivateProfileIntW(L"Hotkeys",L"MenuVirtualKey",VK_INSERT,g_ini.c_str())),8,254);
    g_consoleScan=std::clamp(int(GetPrivateProfileIntW(L"Hotkeys",L"DeveloperConsoleScanCode",41,g_ini.c_str())),1,127);
    g_worldFov=std::clamp(int(GetPrivateProfileIntW(L"Camera",L"WorldFOV",90,g_ini.c_str())),75,130);
    g_worldEnabled=GetPrivateProfileIntW(L"Camera",L"WorldFOVEnabled",0,g_ini.c_str())!=0;
    g_fpsEnabled=GetPrivateProfileIntW(L"Camera",L"FPSFOVEnabled",0,g_ini.c_str())!=0;
    g_fpsFov=std::clamp(int(GetPrivateProfileIntW(L"Camera",L"FPSFOV",85,g_ini.c_str())),60,130);
    Log(L"CRToi V0.4A camera-scoped World/FPS FOV initialized. No global scalar patch.");
}
void PressScan(WORD scan){
    INPUT keys[2]{};
    keys[0].type=INPUT_KEYBOARD;keys[0].ki.wScan=scan;keys[0].ki.dwFlags=KEYEVENTF_SCANCODE;
    keys[1]=keys[0];keys[1].ki.dwFlags|=KEYEVENTF_KEYUP;
    SendInput(2,keys,sizeof(INPUT));
}
void PressVk(WORD vk){
    INPUT keys[2]{};
    keys[0].type=INPUT_KEYBOARD;keys[0].ki.wVk=vk;
    keys[1]=keys[0];keys[1].ki.dwFlags=KEYEVENTF_KEYUP;
    SendInput(2,keys,sizeof(INPUT));
}
void TypeChar(wchar_t c){
    INPUT keys[2]{};
    keys[0].type=INPUT_KEYBOARD;keys[0].ki.wScan=WORD(c);keys[0].ki.dwFlags=KEYEVENTF_UNICODE;
    keys[1]=keys[0];keys[1].ki.dwFlags=KEYEVENTF_UNICODE|KEYEVENTF_KEYUP;
    SendInput(2,keys,sizeof(INPUT));
}
DWORD WINAPI SendWorldFov(void* unused){
    (void)unused;
    // Opt-in only. This sends to the game's OWN console; no code patch.
    Sleep(180);
    if(GetForegroundWindow()!=g_gameHwnd) {
        Log(L"World FOV command not sent because game is not foreground.");
        return 0;
    }
    PressScan(WORD(g_consoleScan.load()));
    Sleep(135);
    wchar_t line[32]{};
    swprintf_s(line,L"fov %d",g_worldFov.load());
    for(const wchar_t* p=line;*p;++p)TypeChar(*p);
    PressVk(VK_RETURN);
    Log(L"World FOV console command sent; engine response cannot be confirmed.");
    return 0;
}
// These native RVAs and struct offsets were verified against the exact game EXE.
// Camera output: FMinimalViewInfo +0x30 World FOV, +0x34 DesiredFOV,
// +0x38 FirstPersonFOV, +0x68 first-person flags (bit 0x2).
// CameraComponent: +0x230 FOV, +0x234 FirstPersonFOV, +0x262 flags bit 0x04.
// Scope/zoom or a different camera are always owned by the game.
// No global PI/360 constant modifications; no executable file writes.
using CameraViewFn=void(__fastcall*)(void*,float,void*);
CameraViewFn g_originalCameraView=nullptr;
constexpr uintptr_t kCameraMethodRva=0x363A3C0;
constexpr uintptr_t kCameraVtableRva=0x7D25378;

bool ScopeInputHeld() {
    if((GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0)return true;
    for(DWORD i=0;i<4;++i) {
        XINPUT_STATE state{};
        if(XInputGetState(i,&state)==ERROR_SUCCESS &&
            state.Gamepad.bLeftTrigger>30)return true;
    }
    return false;
}
void __fastcall HookCameraView(void* camera,float delta,void* out) {
    if(!g_originalCameraView)return;
    g_originalCameraView(camera,delta,out);
    ++g_cameraCalls;
    if(!camera||!out||!g_cameraHookReady.load())return;
    HMODULE exe=GetModuleHandleW(nullptr);
    if(!exe)return;
    const uintptr_t expectedVtable=reinterpret_cast<uintptr_t>(exe)+kCameraVtableRva;
    if(*reinterpret_cast<uintptr_t*>(camera)!=expectedVtable)return;
    const auto* object=reinterpret_cast<const uint8_t*>(camera);
    if((object[0x262]&0x04)==0)return;
    auto* view=reinterpret_cast<uint8_t*>(out);
    uint32_t flags{};
    float baseWorld=0.f,baseFPS=0.f;
    std::memcpy(&flags,view+0x68,4);
    std::memcpy(&baseWorld,view+0x30,4);
    std::memcpy(&baseFPS,view+0x38,4);
    if((flags&0x02)==0 || !std::isfinite(baseWorld)||
       !std::isfinite(baseFPS) || baseWorld<55.f ||baseWorld>125.f ||
       baseFPS<40.f ||baseFPS>150.f)return;
    ++g_firstPersonCandidates;
    bool aiming=ScopeInputHeld();
    const uintptr_t ptr=reinterpret_cast<uintptr_t>(camera);
    if(g_captureCamera.load()&&!aiming) {
        // The player explicitly selects this camera during normal gameplay.
        g_playerCamera.store(ptr);
        g_baselineWorld.store(baseWorld);
        g_baselineFps.store(baseFPS);
        g_nativeWorld.store(baseWorld);
        g_nativeFps.store(baseFPS);
        g_captureCamera=false;
        wchar_t line[240]{};
        swprintf_s(line,L"Captured player camera %p; native world=%.2f, first-person=%.2f",
            camera,double(baseWorld),double(baseFPS));
        Log(line);
    }
    if(g_playerCamera.load()!=ptr)return; // camera swap (cinematics) => vanilla
    g_nativeWorld.store(baseWorld);
    g_nativeFps.store(baseFPS);
    if(g_suspendFov.load())return;
    // Aiming, scope zoom, camera blends, FOV animation or gameplay cameras
    // outside their captured baseline must remain fully under native control.
    if(aiming ||
       std::abs(baseWorld-g_baselineWorld.load())>1.0f ||
       std::abs(baseFPS-g_baselineFps.load())>1.0f) {
        ++g_scopeBypass;
        return;
    }
    if(g_worldEnabled.load()){
        const float value=float(std::clamp(g_worldFov.load(),75,130));
        std::memcpy(view+0x30,&value,4);
        std::memcpy(view+0x34,&value,4);
        ++g_worldApplied;
    }
    if(g_fpsEnabled.load()){
        const float value=float(std::clamp(g_fpsFov.load(),60,130));
        std::memcpy(view+0x38,&value,4);
        ++g_fpsApplied;
    }
}
void InstallCameraHook(){
    auto* exe=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    if(!exe){Log(L"Camera hook: game module unavailable.");return;}
    auto* dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(exe);
    if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<=0||dos->e_lfanew>0x10000){
        Log(L"Camera hook: invalid PE headers.");return;
    }
    auto* pe=reinterpret_cast<const IMAGE_NT_HEADERS64*>(exe+dos->e_lfanew);
    if(pe->Signature!=IMAGE_NT_SIGNATURE||
       pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||
       pe->OptionalHeader.SizeOfImage!=0x0A46F000){
        Log(L"Camera hook: unsupported EXE image size; disabled.");return;
    }
    static const uint8_t prologue[]={
        0x4C,0x8B,0xDC,0x55,0x57,0x49,0x8D,0xAB,0x18,0xFE,0xFF,0xFF
    };
    if(std::memcmp(exe+kCameraMethodRva,prologue,sizeof(prologue))!=0) {
        Log(L"Camera hook: native GetCameraView signature differs; disabled.");return;
    }
    auto* table=reinterpret_cast<const uintptr_t*>(exe+kCameraVtableRva);
    if(table[0x600/8]!=reinterpret_cast<uintptr_t>(exe+0x32BFD10) ||
       table[0x608/8]!=reinterpret_cast<uintptr_t>(exe+0x32BFE70) ||
       table[0x620/8]!=reinterpret_cast<uintptr_t>(exe+kCameraMethodRva)) {
        Log(L"Camera hook: vtable signature differs; disabled.");return;
    }
    if(MH_CreateHook(exe+kCameraMethodRva,reinterpret_cast<void*>(&HookCameraView),
        reinterpret_cast<void**>(&g_originalCameraView))!=MH_OK ||
       MH_EnableHook(exe+kCameraMethodRva)!=MH_OK) {
        Log(L"Camera hook: MinHook error. Camera controls will remain inert.");return;
    }
    g_cameraHookReady=true;
    Log(L"Camera hook verified and enabled: World/FirstPerson view-info output, not scalar patch.");
}

void ResetBuffers(){
    for(auto& f:g_dx12Frames){f.rtv.Reset();f.wrapped.Reset();}
    g_d3d11Rtv.Reset();
}
LRESULT CALLBACK WindowProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp){
    if(msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN){
        if(!(lp & (1LL<<30))){
            const int capture=g_captureHotkey.exchange(0);
            if(capture) {
                if(wp!=VK_ESCAPE) {
                    if(capture==1)g_menuVK=int(wp);
                    if(capture==2)g_consoleScan=int((lp>>16)&0xFF);
                    SaveSettings();
                    Log(L"Hotkey rebound through native game window.");
                }
                return 0;
            }
            if(int(wp)==g_menuVK.load()){
                g_menuOpen=!g_menuOpen.load();
                return 0;
            }
            // Developer-console manipulation is intentionally out of scope
            // for this menu-only test build.
        }
    }
    if(g_menuOpen.load()&&g_imguiReady.load()){
        if(msg==WM_INPUT)return DefWindowProcW(wnd,msg,wp,lp);
        // Win32 WndProc may run on a different UE thread from Present.
        // Queue character and wheel input; consume only on the rendering thread.
        if(msg==WM_MOUSEWHEEL)g_wheelDelta.fetch_add(GET_WHEEL_DELTA_WPARAM(wp));
        if(msg==WM_CHAR) {
            const unsigned w=g_charWrite.load(std::memory_order_relaxed);
            g_chars[w%64].store(unsigned(wp),std::memory_order_relaxed);
            g_charWrite.store(w+1,std::memory_order_release);
        }
        if(msg>=WM_MOUSEFIRST&&msg<=WM_MOUSELAST)return 1;
        if((msg>=WM_KEYFIRST&&msg<=WM_KEYLAST)||msg==WM_CHAR)return 1;
    }
    return g_previousWndProc?CallWindowProcW(g_previousWndProc,wnd,msg,wp,lp):DefWindowProcW(wnd,msg,wp,lp);
}
void DrawMenu(){
    bool open=g_menuOpen.load();
    if(!open)return;
    layout::StyleScope style;
    ImGui::SetNextWindowSize(ImVec2(720,555),ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(520,370),ImVec2(1300,1020));
    if(ImGui::Begin("Crisol Enhanced PC Mod | V0.4A CAMERA-SCOPED TEST",
        &open,ImGuiWindowFlags_NoCollapse)){
        ImGui::TextColored(ImVec4(.52f,.83f,1.f,1.f),"CRISOL ENHANCED PC MOD");
        ImGui::SameLine();ImGui::TextDisabled("V0.4A | CAMERA-SCOPED FOV");
        ImGui::Separator();ImGui::Spacing();
        ImGui::SetNextItemOpen(true,ImGuiCond_Once);
        if(layout::Group("Camera / Gameplay ownership")){
            if(!g_cameraHookReady.load()){
                ImGui::TextColored(ImVec4(1.f,.72f,.42f,1.f),
                    "Camera hook unavailable; this EXE does not match the verified target.");
            }else if(g_playerCamera.load()==0){
                ImGui::TextColored(ImVec4(1.f,.78f,.40f,1.f),
                    "No gameplay camera captured. Enter normal gameplay first.");
            }else{
                ImGui::TextColored(ImVec4(.45f,.90f,.69f,1.f),
                    "Camera captured. Scoped edits are restricted to this instance.");
                ImGui::Text("Original World FOV: %.1f | Original FPS FOV: %.1f",
                    g_baselineWorld.load(),g_baselineFps.load());
            }
            ImGui::TextWrapped("Capture only during normal gameplay, outside ADS/zoom and cinematics. Other cameras are never changed.");
            if(ImGui::Button(g_captureCamera.load()?"Waiting for active camera...":"Capture gameplay camera")){
                g_captureCamera=true;
                g_playerCamera=0;
            }
            ImGui::SameLine();
            if(ImGui::Button("Release")){g_playerCamera=0;g_captureCamera=false;}
            ImGui::Spacing();
        }
        if(layout::Group("Camera / Field of View")){
            bool world=g_worldEnabled.load();
            if(ImGui::Checkbox("World FOV override",&world)){
                g_worldEnabled=world;SaveSettings();
            }
            ImGui::TextDisabled("World view only. Original FOV wins during zoom, scope or camera changes.");
            if(world){
                float value=float(g_worldFov.load());
                if(layout::NumberRow("World FOV","WorldFOV",&value,75.f,130.f,90.f,
                    "%.0f deg","%.0f")){
                    g_worldFov=int(std::lround(value));SaveSettings();
                }
            }
            ImGui::Spacing();
            bool fps=g_fpsEnabled.load();
            if(ImGui::Checkbox("FPS / Weapon FOV override",&fps)){
                g_fpsEnabled=fps;SaveSettings();
            }
            ImGui::TextDisabled("Separate Unreal first-person rendering FOV; only when the game's native first-person flag is active.");
            if(fps){
                float value=float(g_fpsFov.load());
                if(layout::NumberRow("FPS / Weapon FOV","FPSFOV",&value,60.f,130.f,85.f,
                    "%.0f deg","%.0f")){
                    g_fpsFov=int(std::lround(value));SaveSettings();
                }
            }
            ImGui::Spacing();
            bool suspended=g_suspendFov.load();
            if(ImGui::Checkbox("Suspend ALL overrides (vanilla)",&suspended)){
                g_suspendFov=suspended;
            }
            if(ImGui::Button("Restore vanilla settings")){
                g_worldEnabled=false;g_fpsEnabled=false;
                g_worldFov=90;g_fpsFov=85;
                g_suspendFov=false;SaveSettings();
            }
        }
        if(layout::Group("Controls")){
            ImGui::Text("Toggle overlay: virtual key %d",g_menuVK.load());
            if(ImGui::Button(g_captureHotkey.load()==1?
                "Press a key (Esc to cancel)##menu":"Rebind menu key"))
                g_captureHotkey=1;
            ImGui::SameLine();
            if(ImGui::Button("Reset to Insert")){
                g_menuVK=VK_INSERT;g_captureHotkey=0;SaveSettings();
            }
        }
        if(layout::Group("About / Diagnostics")){
            ImGui::Text("Renderer: %s",g_isDx12?"DX12 (D3D11On12)":"DX11");
            ImGui::Text("GetCameraView calls: %u | eligible first-person: %u",
                g_cameraCalls.load(),g_firstPersonCandidates.load());
            ImGui::Text("World changes: %u | FPS changes: %u | zoom bypass: %u",
                g_worldApplied.load(),g_fpsApplied.load(),g_scopeBypass.load());
            ImGui::TextDisabled("Camera must be captured per session or after level reload.");
            ImGui::TextDisabled("Cinema using same player camera needs in-game validation.");
            ImGui::TextDisabled("No developer console; no global scalar patch.");
        }
    }
    ImGui::End();
    g_menuOpen=open;
}
bool PrepareRtv(ID3D11RenderTargetView** out,ID3D11Texture2D** wrapped){
    *out=nullptr;*wrapped=nullptr;
    if(!g_isDx12) {
        if(!g_d3d11Rtv) {
            ComPtr<ID3D11Texture2D> back;
            if(FAILED(g_swap->GetBuffer(0,IID_PPV_ARGS(&back))))return false;
            if(FAILED(g_device->CreateRenderTargetView(back.Get(),nullptr,&g_d3d11Rtv)))return false;
        }
        *out=g_d3d11Rtv.Get();return true;
    }
    if(!g_swap3||!g_on12)return false;
    UINT index=g_swap3->GetCurrentBackBufferIndex();
    if(index>=8)return false;
    auto& f=g_dx12Frames[index];
    if(!f.rtv) {
        ComPtr<ID3D12Resource> back12;
        if(FAILED(g_swap->GetBuffer(index,IID_PPV_ARGS(&back12))))return false;
        D3D11_RESOURCE_FLAGS flags{};
        flags.BindFlags=D3D11_BIND_RENDER_TARGET;
        if(FAILED(g_on12->CreateWrappedResource(back12.Get(),&flags,
            D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT,
            IID_PPV_ARGS(&f.wrapped))))return false;
        if(FAILED(g_device->CreateRenderTargetView(f.wrapped.Get(),nullptr,&f.rtv))){
            f.wrapped.Reset();return false;
        }
    }
    *out=f.rtv.Get();*wrapped=f.wrapped.Get();
    return true;
}
bool InitUi(IDXGISwapChain* swap){
    DXGI_SWAP_CHAIN_DESC desc{};
    if(FAILED(swap->GetDesc(&desc))||!desc.OutputWindow){
        Log(L"UI init postponed: swap chain has no HWND.");return false;
    }
    HWND hwnd=desc.OutputWindow;
    DWORD pid{};
    GetWindowThreadProcessId(hwnd,&pid);
    if(pid!=GetCurrentProcessId())return false;
    if(desc.BufferDesc.Width&&desc.BufferDesc.Width<480)return false;
    g_gameHwnd=hwnd;
    g_isDx12=false;
    if(SUCCEEDED(swap->GetDevice(IID_PPV_ARGS(&g_device)))){
        g_device->GetImmediateContext(&g_context);
        Log(L"DirectX 11 device captured.");
    }else if(g_queue){
        ComPtr<ID3D12Device> device12;
        if(FAILED(g_queue->GetDevice(IID_PPV_ARGS(&device12)))){
            Log(L"DX12 command queue has no compatible device.");return false;
        }
        IUnknown* queues[]={g_queue.Get()};
        HRESULT hr=D3D11On12CreateDevice(device12.Get(),D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            nullptr,0,queues,1,0,&g_device,&g_context,nullptr);
        if(FAILED(hr)){
            Log(L"D3D11On12CreateDevice failed.");return false;
        }
        if(FAILED(g_device.As(&g_on12))||FAILED(swap->QueryInterface(IID_PPV_ARGS(&g_swap3)))){
            Log(L"DX12 wrapped resource interfaces unavailable.");return false;
        }
        g_isDx12=true;
        Log(L"DirectX 12 command queue captured; D3D11On12 initialized.");
    }else{
        Log(L"No DX11 device or captured DX12 command queue. Overlay disabled.");
        return false;
    }
    if(!g_device||!g_context)return false;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename=nullptr;
    if(!ImGui_ImplWin32_Init(hwnd)||!ImGui_ImplDX11_Init(g_device.Get(),g_context.Get())){
        Log(L"ImGui backend initialization failed.");
        return false;
    }
    g_previousWndProc=reinterpret_cast<WNDPROC>(
        SetWindowLongPtrW(hwnd,GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(WindowProc)));
    if(!g_previousWndProc){
        Log(L"WndProc hook failed; refusing input capture.");
        return false;
    }
    g_imguiReady=true;
    Log(L"Overlay V0.2A initialized (in-process DXGI Present). Press Insert.");
    return true;
}
void Render(IDXGISwapChain* swap){
    if(g_failed||swap!=g_swap.Get())return;

    if(!g_imguiReady) {
        static int attempts=0;
        if(++attempts>20){g_failed=true;Log(L"Overlay init attempted 20 times; fail-open.");return;}
        if(!InitUi(swap))return;
    }
    if(!g_menuOpen.load())return;
    ID3D11RenderTargetView* rtv=nullptr;
    ID3D11Texture2D* wrapped=nullptr;
    if(!PrepareRtv(&rtv,&wrapped))return;
    if(wrapped) {
        ID3D11Resource* resources[]={wrapped};
        g_on12->AcquireWrappedResources(resources,1);
    }
    // Keep software pointer usable over an exclusive/fullscreen game.
    // Do not alter swap-chain fullscreen state.
    ClipCursor(nullptr);
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGuiIO& io=ImGui::GetIO();
    io.MouseDrawCursor=true;
    io.AddMouseButtonEvent(0,(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0);
    io.AddMouseButtonEvent(1,(GetAsyncKeyState(VK_RBUTTON)&0x8000)!=0);
    io.AddMouseButtonEvent(2,(GetAsyncKeyState(VK_MBUTTON)&0x8000)!=0);
    const int wheel=g_wheelDelta.exchange(0);
    if(wheel)io.AddMouseWheelEvent(0.0f,float(wheel)/float(WHEEL_DELTA));
    struct Key {int vk;ImGuiKey key;};
    static const Key keys[]={
        {VK_TAB,ImGuiKey_Tab},{VK_LEFT,ImGuiKey_LeftArrow},
        {VK_RIGHT,ImGuiKey_RightArrow},{VK_UP,ImGuiKey_UpArrow},
        {VK_DOWN,ImGuiKey_DownArrow},{VK_RETURN,ImGuiKey_Enter},
        {VK_ESCAPE,ImGuiKey_Escape},{VK_BACK,ImGuiKey_Backspace},
        {VK_DELETE,ImGuiKey_Delete},{VK_HOME,ImGuiKey_Home},
        {VK_END,ImGuiKey_End},{VK_SHIFT,ImGuiKey_ModShift},
        {VK_CONTROL,ImGuiKey_ModCtrl},{VK_MENU,ImGuiKey_ModAlt},
        {'0',ImGuiKey_0},{'1',ImGuiKey_1},{'2',ImGuiKey_2},
        {'3',ImGuiKey_3},{'4',ImGuiKey_4},{'5',ImGuiKey_5},
        {'6',ImGuiKey_6},{'7',ImGuiKey_7},{'8',ImGuiKey_8},
        {'9',ImGuiKey_9}
    };
    for(const Key& k:keys)io.AddKeyEvent(k.key,(GetAsyncKeyState(k.vk)&0x8000)!=0);
    unsigned read=g_charRead.load(std::memory_order_relaxed);
    const unsigned write=g_charWrite.load(std::memory_order_acquire);
    if(write-read>64)read=write-64;
    while(read<write){
        io.AddInputCharacterUTF16(ImWchar16(g_chars[read%64].load(std::memory_order_relaxed)));
        ++read;
    }
    g_charRead.store(read,std::memory_order_relaxed);
    ImGui::NewFrame();
    DrawMenu();
    ImGui::Render();
    ComPtr<ID3D11RenderTargetView> prior;
    ComPtr<ID3D11DepthStencilView> oldDepth;
    g_context->OMGetRenderTargets(1,&prior,&oldDepth);
    g_context->OMSetRenderTargets(1,&rtv,nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    ID3D11RenderTargetView* oldRtv=prior.Get();
    g_context->OMSetRenderTargets(1,&oldRtv,oldDepth.Get());
    if(wrapped) {
        ID3D11Resource* resources[]={wrapped};
        g_on12->ReleaseWrappedResources(resources,1);
        g_context->Flush();
    }
}
HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain*,UINT,UINT);
HRESULT STDMETHODCALLTYPE HookPresent1(IDXGISwapChain1*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*);
HRESULT STDMETHODCALLTYPE HookResize(IDXGISwapChain*,UINT,UINT,UINT,DXGI_FORMAT,UINT);
void OnCreated(IDXGISwapChain* swap,IUnknown* device){
    if(!swap)return;
    g_swap=swap;
    if(device&&!g_queue) {
        ComPtr<ID3D12CommandQueue> queue;
        if(SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&queue)))){
            g_queue=queue;Log(L"DX12 native command queue observed at DXGI creation.");
        }
    }
    auto** vt=*reinterpret_cast<void***>(swap);
    if(!g_hookedPresent) {
        void* target=vt[8];
        if(MH_CreateHook(target,reinterpret_cast<void*>(&HookPresent),
            reinterpret_cast<void**>(&g_originalPresent))==MH_OK&&MH_EnableHook(target)==MH_OK){
            g_hookedPresent=target;
        }
    }
    if(!g_hookedResize) {
        void* target=vt[13];
        if(MH_CreateHook(target,reinterpret_cast<void*>(&HookResize),
            reinterpret_cast<void**>(&g_originalResize))==MH_OK&&MH_EnableHook(target)==MH_OK){
            g_hookedResize=target;
        }
    }
    ComPtr<IDXGISwapChain1> sc1;
    if(SUCCEEDED(swap->QueryInterface(IID_PPV_ARGS(&sc1)))&&!g_hookedPresent1){
        auto** v1=*reinterpret_cast<void***>(sc1.Get());
        void* target=v1[22];
        if(MH_CreateHook(target,reinterpret_cast<void*>(&HookPresent1),
            reinterpret_cast<void**>(&g_originalPresent1))==MH_OK&&MH_EnableHook(target)==MH_OK){
            g_hookedPresent1=target;
        }
    }
    Log(L"Swap chain tracked; Present/Present1/Resize hooks requested.");
}
HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swap,UINT interval,UINT flags){
    thread_local bool busy=false;
    if(!busy){busy=true;Render(swap);busy=false;}
    return g_originalPresent?g_originalPresent(swap,interval,flags):DXGI_ERROR_INVALID_CALL;
}
HRESULT STDMETHODCALLTYPE HookPresent1(IDXGISwapChain1* swap,UINT interval,UINT flags,const DXGI_PRESENT_PARAMETERS* p){
    thread_local bool busy=false;
    if(!busy){busy=true;Render(swap);busy=false;}
    return g_originalPresent1?g_originalPresent1(swap,interval,flags,p):DXGI_ERROR_INVALID_CALL;
}
HRESULT STDMETHODCALLTYPE HookResize(IDXGISwapChain* swap,UINT count,UINT w,UINT h,DXGI_FORMAT fmt,UINT flags){
    if(swap==g_swap.Get()){ResetBuffers();Log(L"Swap chain resize: overlay resources released.");}
    return g_originalResize?g_originalResize(swap,count,w,h,fmt,flags):DXGI_ERROR_INVALID_CALL;
}
HRESULT STDMETHODCALLTYPE HookFactorySwap(IDXGIFactory* f,IUnknown* dev,DXGI_SWAP_CHAIN_DESC* d,IDXGISwapChain** out){
    HRESULT hr=g_originalFactorySwap?g_originalFactorySwap(f,dev,d,out):E_FAIL;
    if(SUCCEEDED(hr)&&out&&*out)OnCreated(*out,dev);
    return hr;
}
HRESULT STDMETHODCALLTYPE HookFactoryHwnd(IDXGIFactory2* f,IUnknown* dev,HWND wnd,
    const DXGI_SWAP_CHAIN_DESC1* d,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* full,IDXGIOutput* restrictTo,IDXGISwapChain1** out){
    HRESULT hr=g_originalFactoryHwnd?g_originalFactoryHwnd(f,dev,wnd,d,full,restrictTo,out):E_FAIL;
    if(SUCCEEDED(hr)&&out&&*out)OnCreated(*out,dev);
    return hr;
}
HRESULT STDMETHODCALLTYPE HookFactoryComposition(IDXGIFactory2* f,IUnknown* dev,
    const DXGI_SWAP_CHAIN_DESC1* d,IDXGIOutput* restrictTo,IDXGISwapChain1** out){
    HRESULT hr=g_originalFactoryComposition?g_originalFactoryComposition(f,dev,d,restrictTo,out):E_FAIL;
    if(SUCCEEDED(hr)&&out&&*out)OnCreated(*out,dev);
    return hr;
}
void InstallFactoryHooks(IUnknown* factory){
    if(!factory)return;
    ComPtr<IDXGIFactory> base;
    if(FAILED(factory->QueryInterface(IID_PPV_ARGS(&base))))return;
    auto** vt=*reinterpret_cast<void***>(base.Get());
    if(!g_hookedFactorySwap) {
        void* target=vt[10];
        if(MH_CreateHook(target,reinterpret_cast<void*>(&HookFactorySwap),
            reinterpret_cast<void**>(&g_originalFactorySwap))==MH_OK&&MH_EnableHook(target)==MH_OK)
            g_hookedFactorySwap=target;
    }
    ComPtr<IDXGIFactory2> factory2;
    if(SUCCEEDED(factory->QueryInterface(IID_PPV_ARGS(&factory2)))){
        auto** v2=*reinterpret_cast<void***>(factory2.Get());
        if(!g_hookedFactoryHwnd) {
            void* target=v2[15];
            if(MH_CreateHook(target,reinterpret_cast<void*>(&HookFactoryHwnd),
                reinterpret_cast<void**>(&g_originalFactoryHwnd))==MH_OK&&MH_EnableHook(target)==MH_OK)
                g_hookedFactoryHwnd=target;
        }
        if(!g_hookedFactoryComposition) {
            void* target=v2[24];
            if(MH_CreateHook(target,reinterpret_cast<void*>(&HookFactoryComposition),
                reinterpret_cast<void**>(&g_originalFactoryComposition))==MH_OK&&MH_EnableHook(target)==MH_OK)
                g_hookedFactoryComposition=target;
        }
    }
    Log(L"IDXGIFactory hooks installed / reused.");
}
} // namespace

extern "C" __declspec(dllexport) void WINAPI CRToi_Initialize() {
    std::call_once(g_initOnce,[] {
        InitPathAndSettings();
        MH_STATUS status=MH_Initialize();
        if(status!=MH_OK&&status!=MH_ERROR_ALREADY_INITIALIZED){
            g_failed=true;Log(L"MinHook initialization failed.");
        } else {
            InstallCameraHook();
        }
    });
}
extern "C" __declspec(dllexport) void WINAPI CRToi_OnFactory(IUnknown* factory) {
    CRToi_Initialize();
    if(!g_failed)InstallFactoryHooks(factory);
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH){g_module=module;DisableThreadLibraryCalls(module);}
    return TRUE;
}
