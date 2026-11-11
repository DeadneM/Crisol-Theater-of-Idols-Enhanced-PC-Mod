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
#include <cwctype>
#include <cstdlib>
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
std::atomic_bool g_skipStartupVideos{true};
std::atomic_uint g_introVideoOpensSuppressed{0};
std::atomic_bool g_introHookWReady{false},g_introHook2Ready{false};
std::atomic_int g_captureHotkey{0},g_menuVK{VK_INSERT},g_consoleScan{0x29};
std::atomic_int g_cheatShopVK{VK_F1},g_smlConsoleVK{0};
std::atomic_int g_flyVK{VK_F2},g_teleportVK{VK_F3},g_flySpeed{600};
std::atomic_bool g_flyActive{false},g_flyNeedsOrigin{true},g_teleportPending{false};
std::atomic_uint g_flyFrames{0},g_teleportSuccess{0},g_teleportFailed{0};
std::atomic_uintptr_t g_flyOwner{0};
// The following location arrays are used only by the existing camera-hook thread.
double g_flyPosition[3]{},g_flyCameraOffset[3]{},g_flyStartOwner[3]{};
ULONGLONG g_flyLastTick=0;

std::atomic_bool g_cheatShopBusy{false};
std::atomic_uint g_cheatShopAttempts{0},g_cheatShopSequences{0};

std::atomic_int g_worldFov{90},g_fpsFov{85};
std::atomic_bool g_worldEnabled{false},g_fpsEnabled{false};
std::atomic_bool g_captureCamera{false},g_autoCamera{true};
std::atomic_uint g_autoCameraCaptures{0},g_autoCameraReleases{0};
std::atomic_uintptr_t g_autoCandidate{0};
std::atomic_uint64_t g_autoCandidateSince{0},g_playerLastSeen{0};
std::atomic_uint g_autoCandidateFrames{0};
std::atomic<float> g_candidateWorld{0.f},g_candidateFPS{0.f};
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
    swprintf_s(v,L"%d",g_cheatShopVK.load());WritePrivateProfileStringW(L"Hotkeys",L"CheatShopVirtualKey",v,g_ini.c_str());
    swprintf_s(v,L"%d",g_flyVK.load());WritePrivateProfileStringW(L"Hotkeys",L"FreecamVirtualKey",v,g_ini.c_str());
    swprintf_s(v,L"%d",g_teleportVK.load());WritePrivateProfileStringW(L"Hotkeys",L"TeleportVirtualKey",v,g_ini.c_str());
    swprintf_s(v,L"%d",g_flySpeed.load());WritePrivateProfileStringW(L"Freecam",L"Speed",v,g_ini.c_str());
    swprintf_s(v,L"%d",g_consoleScan.load());WritePrivateProfileStringW(L"Hotkeys",L"DeveloperConsoleScanCode",v,g_ini.c_str());
    swprintf_s(v,L"%d",g_worldFov.load());WritePrivateProfileStringW(L"Camera",L"WorldFOV",v,g_ini.c_str());
    WritePrivateProfileStringW(L"Camera",L"WorldFOVEnabled",g_worldEnabled.load()?L"1":L"0",g_ini.c_str());
    WritePrivateProfileStringW(L"Camera",L"FPSFOVEnabled",g_fpsEnabled.load()?L"1":L"0",g_ini.c_str());
    WritePrivateProfileStringW(L"Camera",L"AutoCaptureGameplay",g_autoCamera.load()?L"1":L"0",g_ini.c_str());
    WritePrivateProfileStringW(L"Startup",L"SkipIntroVideos",g_skipStartupVideos.load()?L"1":L"0",g_ini.c_str());
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
    g_cheatShopVK=std::clamp(int(GetPrivateProfileIntW(L"Hotkeys",L"CheatShopVirtualKey",VK_F1,g_ini.c_str())),8,254);
    g_flyVK=std::clamp(int(GetPrivateProfileIntW(L"Hotkeys",L"FreecamVirtualKey",VK_F2,g_ini.c_str())),8,254);
    g_teleportVK=std::clamp(int(GetPrivateProfileIntW(L"Hotkeys",L"TeleportVirtualKey",VK_F3,g_ini.c_str())),8,254);
    g_flySpeed=std::clamp(int(GetPrivateProfileIntW(L"Freecam",L"Speed",600,g_ini.c_str())),100,2000);
    g_smlConsoleVK=std::clamp(int(GetPrivateProfileIntW(L"Hotkeys",L"SMLConsoleVirtualKey",0,g_ini.c_str())),0,254);
    g_consoleScan=std::clamp(int(GetPrivateProfileIntW(L"Hotkeys",L"DeveloperConsoleScanCode",41,g_ini.c_str())),1,127);
    g_worldFov=std::clamp(int(GetPrivateProfileIntW(L"Camera",L"WorldFOV",90,g_ini.c_str())),75,130);
    g_worldEnabled=GetPrivateProfileIntW(L"Camera",L"WorldFOVEnabled",0,g_ini.c_str())!=0;
    g_fpsEnabled=GetPrivateProfileIntW(L"Camera",L"FPSFOVEnabled",0,g_ini.c_str())!=0;
    g_autoCamera=GetPrivateProfileIntW(L"Camera",L"AutoCaptureGameplay",1,g_ini.c_str())!=0;
    g_skipStartupVideos=GetPrivateProfileIntW(L"Startup",L"SkipIntroVideos",1,g_ini.c_str())!=0;
    g_fpsFov=std::clamp(int(GetPrivateProfileIntW(L"Camera",L"FPSFOV",85,g_ini.c_str())),60,130);
    Log(L"CRToi V0.8B: game-window CheatShop input; redundant suspend control removed; scoped FOV hooks retained.");
}
// Suppress ONLY the three documented startup MP4 files in Crisol's
// Content/Movies directory while the game is initializing. This avoids
// replacing/renaming game files, packing third-party assets or affecting
// story/cutscene videos. Other file I/O is always delegated unmodified.
// The boot-time movie pipeline may have already opened a movie before DXGI
// initialization: in that case this feature will not skip it (fail-open).
using CreateFileWFn=HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,LPSECURITY_ATTRIBUTES,
                                    DWORD,DWORD,HANDLE);
using CreateFile2Fn=HANDLE(WINAPI*)(LPCWSTR,DWORD,DWORD,DWORD,
                                    LPCREATEFILE2_EXTENDED_PARAMETERS);
CreateFileWFn g_realCreateFileW=nullptr;
CreateFile2Fn g_realCreateFile2=nullptr;
bool TargetStartupMP4(LPCWSTR filename){
    if(!filename||!g_skipStartupVideos.load())return false;
    const size_t n=wcslen(filename);
    if(n<12 || n>32768)return false;
    const wchar_t* name=filename;
    for(const wchar_t* p=filename;*p;++p)
        if(*p==L'\\'||*p==L'/')name=p+1;
    if(_wcsicmp(name,L"BlumLogo.mp4")!=0 &&
       _wcsicmp(name,L"AnimacionLogoNew.mp4")!=0 &&
       _wcsicmp(name,L"SplashVideo1.mp4")!=0)return false;
    std::wstring location(filename,name);
    for(wchar_t& c:location) {
        if(c==L'\\')c=L'/';
        else c=static_cast<wchar_t>(towlower(c));
    }
    return location.ends_with(L"/content/movies/") ||
           location.ends_with(L"/movies/") ||
           location.empty(); // allow relative single-file opening
}
bool ShouldHideIntro(LPCWSTR filename,DWORD access,DWORD disposition){
    return TargetStartupMP4(filename) && disposition==OPEN_EXISTING &&
           ((access&(GENERIC_READ|FILE_READ_DATA|FILE_EXECUTE))!=0 || access==0);
}
HANDLE WINAPI HookCreateFileW(LPCWSTR file,DWORD access,DWORD share,
    LPSECURITY_ATTRIBUTES security,DWORD create,DWORD flags,HANDLE templateFile){
    if(ShouldHideIntro(file,access,create)){
        ++g_introVideoOpensSuppressed;
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    return g_realCreateFileW?
        g_realCreateFileW(file,access,share,security,create,flags,templateFile):
        INVALID_HANDLE_VALUE;
}
HANDLE WINAPI HookCreateFile2(LPCWSTR file,DWORD access,DWORD share,DWORD create,
                              LPCREATEFILE2_EXTENDED_PARAMETERS params){
    if(ShouldHideIntro(file,access,create)){
        ++g_introVideoOpensSuppressed;
        SetLastError(ERROR_FILE_NOT_FOUND);
        return INVALID_HANDLE_VALUE;
    }
    return g_realCreateFile2?g_realCreateFile2(file,access,share,create,params):
        INVALID_HANDLE_VALUE;
}
void InstallIntroVideoHooks(){
    if(!g_skipStartupVideos.load())return;
    HMODULE kernel=GetModuleHandleW(L"kernelbase.dll");
    if(!kernel)kernel=GetModuleHandleW(L"kernel32.dll");
    if(!kernel){Log(L"Skip Intro: Win32 file API not loaded; fail-open.");return;}
    FARPROC createW=GetProcAddress(kernel,"CreateFileW");
    if(createW&&MH_CreateHook(reinterpret_cast<void*>(createW),
       reinterpret_cast<void*>(&HookCreateFileW),
       reinterpret_cast<void**>(&g_realCreateFileW))==MH_OK&&
       MH_EnableHook(reinterpret_cast<void*>(createW))==MH_OK){
        g_introHookWReady=true;
    }
    FARPROC create2=GetProcAddress(kernel,"CreateFile2");
    if(create2&&MH_CreateHook(reinterpret_cast<void*>(create2),
       reinterpret_cast<void*>(&HookCreateFile2),
       reinterpret_cast<void**>(&g_realCreateFile2))==MH_OK&&
       MH_EnableHook(reinterpret_cast<void*>(create2))==MH_OK){
        g_introHook2Ready=true;
    }
    wchar_t info[160]{};
    swprintf_s(info,L"Skip Intro: CreateFileW hook=%d, CreateFile2 hook=%d. Filters three startup MP4 only.",
       int(g_introHookWReady.load()),int(g_introHook2Ready.load()));
    Log(info);
}

// SML's cheatshop is an external mod console command. This small, opt-in
// user-input macro invokes it on one F1 press. It does NOT patch/copy SML.
// This first test does open the UE console briefly, automatically;
// a truly console-free native Exec bridge remains separate future work.
// Queue input to the game's own window. V0.8A's global SendInput sequence
// reported successful injection even when Unreal did not accept the command.
// WM_CHAR supplies actual text instead of synthetic VK_PACKET input.
bool PostShopKey(WORD vk){
    if(!g_gameHwnd||GetForegroundWindow()!=g_gameHwnd)return false;
    const UINT scan=MapVirtualKeyW(vk,MAPVK_VK_TO_VSC);
    const LPARAM down=1|(LPARAM(scan)<<16);
    const bool pressed=PostMessageW(g_gameHwnd,WM_KEYDOWN,vk,down)!=FALSE;
    const bool released=PostMessageW(g_gameHwnd,WM_KEYUP,vk,
        down|(LPARAM(1)<<30)|(LPARAM(1)<<31))!=FALSE;
    return pressed&&released;
}
// SML can be configured with Steam launch argument -consolekey=F10.
// Respect F1..F12 without requiring users to duplicate that setting in INI.
// Explicit SMLConsoleVirtualKey in our INI overrides this detection.
int SMLLaunchConsoleVK(){
    std::wstring command(GetCommandLineW());
    for(wchar_t& c:command)c=static_cast<wchar_t>(towlower(c));
    const auto pos=command.find(L"-consolekey=");
    if(pos==std::wstring::npos)return 0;
    const wchar_t* key=command.c_str()+pos+wcslen(L"-consolekey=");
    if(*key!=L'f')return 0;
    wchar_t* end=nullptr;
    const long number=wcstol(key+1,&end,10);
    if(end==key+1||number<1||number>12)return 0;
    if(*end && !iswspace(*end) && *end!=L'"')return 0;
    return VK_F1+int(number)-1;
}
DWORD WINAPI RunCheatShop(void*){
    // The ImGui menu can trigger this too; let its input interception end.
    Sleep(170);
    if(!g_gameHwnd || GetForegroundWindow()!=g_gameHwnd){
        Log(L"CheatShop: ignored; the game window is not foreground.");
        g_cheatShopBusy=false;
        return 0;
    }
    const int configured=g_smlConsoleVK.load();
    const int launchVK=SMLLaunchConsoleVK();
    const int openVK=configured?configured:(launchVK?launchVK:VK_OEM_3);
    const bool opened=PostShopKey(WORD(openVK));
    if(!opened){
        Log(L"CheatShop: failed to inject console hotkey.");
        g_cheatShopBusy=false;
        return 0;
    }
    // Delay by a few rendered frames so Slate's console has focus.
    Sleep(350);
    if(GetForegroundWindow()!=g_gameHwnd){
        Log(L"CheatShop: cancelled; game lost focus before typing.");
        g_cheatShopBusy=false;
        return 0;
    }
    constexpr wchar_t command[]=L"cheatshop";
    bool sent=true;
    for(const wchar_t* p=command;*p;++p){
        if(GetForegroundWindow()!=g_gameHwnd||
           !PostMessageW(g_gameHwnd,WM_CHAR,WPARAM(*p),1)){sent=false;break;}
        Sleep(35);
    }
    if(sent){
        Sleep(80);
        sent=PostShopKey(VK_RETURN);
    }
    if(sent){
        ++g_cheatShopSequences;
        Log(L"CheatShop: command queued to game window (WM_CHAR). SML response still requires gameplay validation.");
    }else{
        Log(L"CheatShop: keyboard injection incomplete.");
    }
    Sleep(260);
    g_cheatShopBusy=false;
    return 0;
}
void RequestCheatShop(){
    if(g_cheatShopBusy.exchange(true))return;
    ++g_cheatShopAttempts;
    HANDLE h=CreateThread(nullptr,0,&RunCheatShop,nullptr,0,nullptr);
    if(h)CloseHandle(h);
    else{
        Log(L"CheatShop: could not start shortcut dispatcher.");
        g_cheatShopBusy=false;
    }
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
// v0.7A: only the game's already-verified UCameraComponent GetCameraView is used.
// Static analysis on the exact supported EXE verified:
// UActorComponent::GetOwner: pointer at component+0x90 (execGetOwner RVA 0x3702010).
// AActor::K2_GetActorLocation: RootComponent at actor+0x1B8 and world
// FVector3d at root+0x1F0 (execK2_GetActorLocation RVA 0x34C1330).
// AActor::execK2_TeleportTo RVA 0x34C2810 dispatches through actor's vtable
// +0x5D8 with (Actor*, FVector3d*, FRotator3d*, false, false).
// Actor coordinates are changed ONLY by this native collision-tested method,
// never by writing speculative actor offsets or altering saved game data.
bool IsReadableRange(const void* ptr,size_t amount){
    if(!ptr||!amount)return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if(VirtualQuery(ptr,&mbi,sizeof(mbi))!=sizeof(mbi)||
       mbi.State!=MEM_COMMIT || (mbi.Protect&(PAGE_GUARD|PAGE_NOACCESS)))return false;
    const DWORD p=mbi.Protect&0xff;
    if(p!=PAGE_READONLY&&p!=PAGE_READWRITE&&p!=PAGE_WRITECOPY&&
       p!=PAGE_EXECUTE_READ&&p!=PAGE_EXECUTE_READWRITE&&p!=PAGE_EXECUTE_WRITECOPY)
        return false;
    const uintptr_t from=reinterpret_cast<uintptr_t>(ptr);
    const uintptr_t begin=reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    return from>=begin && from-begin<=mbi.RegionSize &&
           amount<=mbi.RegionSize-(from-begin);
}
bool Valid3(const double* v){
    for(int i=0;i<3;++i)
        if(!std::isfinite(v[i])||std::abs(v[i])>1.e8)return false;
    return true;
}
bool GetPlayerRootFromCamera(void* camera,void*& actor,double root[3]){
    actor=nullptr;
    auto* comp=static_cast<const uint8_t*>(camera);
    if(!IsReadableRange(comp,0x98))return false;
    void* ptr=nullptr;
    std::memcpy(&ptr,comp+0x90,sizeof(ptr));
    if(!IsReadableRange(ptr,0x1C0))return false;
    auto* object=static_cast<const uint8_t*>(ptr);
    void* rootComponent=nullptr;
    std::memcpy(&rootComponent,object+0x1B8,sizeof(rootComponent));
    if(!IsReadableRange(rootComponent,0x208))return false;
    std::memcpy(root,static_cast<const uint8_t*>(rootComponent)+0x1F0,24);
    if(!Valid3(root))return false;
    actor=ptr;
    return true;
}
void StopFly(const wchar_t* why){
    if(g_flyActive.exchange(false)){
        g_teleportPending=false;
        g_flyNeedsOrigin=true;
        g_flyOwner=0;
        if(why)Log(why);
    }
}
void ToggleFly(){
    if(g_flyActive.load()){
        StopFly(L"F2: freecam OFF, original camera restored.");
        return;
    }
    if(!g_cameraHookReady.load()||!g_playerCamera.load()){
        Log(L"F2: capture the normal gameplay camera in ImGui before activating freecam.");
        return;
    }
    g_flyNeedsOrigin=true;
    g_teleportPending=false;
    g_flyActive=true;
    Log(L"F2: freecam requested. Player actor will remain in place.");
}
void QueueTeleport(){
    if(!g_flyActive.load())return;
    if(g_teleportPending.exchange(true))return;
    Log(L"F3: teleport requested; waiting for the verified gameplay camera callback.");
}
bool FlyKey(int vk){return (GetAsyncKeyState(vk)&0x8000)!=0;}
bool GameplayWindowActive(){
    return g_gameHwnd&&GetForegroundWindow()==g_gameHwnd&&!g_menuOpen.load();
}
void UpdateFlightLocation(const uint8_t* view){
    const ULONGLONG now=GetTickCount64();
    if(!GameplayWindowActive()){g_flyLastTick=now;return;}
    double dt=g_flyLastTick?double(now-g_flyLastTick)/1000.0:0.0;
    g_flyLastTick=now;
    if(!(dt>0.0&&dt<1.0))return;
    dt=std::min(dt,0.05);
    double rot[3]{};
    std::memcpy(rot,view+0x18,sizeof(rot));
    if(!Valid3(rot))return;
    constexpr double d=0.017453292519943295;
    const double yaw=rot[1]*d,pitch=rot[0]*d;
    const double c=std::cos(yaw),q=std::sin(yaw),cp=std::cos(pitch),sp=std::sin(pitch);
    const int w=int(FlyKey('W')||FlyKey('Z')||FlyKey(VK_UP))-int(FlyKey('S')||FlyKey(VK_DOWN));
    const int a=int(FlyKey('D')||FlyKey(VK_RIGHT))-int(FlyKey('A')||FlyKey('Q')||FlyKey(VK_LEFT));
    const int z=int(FlyKey(VK_SPACE))-int(FlyKey(VK_CONTROL));
    const int squared=w*w+a*a+z*z;
    if(!squared)return;
    const double speed=double(g_flySpeed.load())*(FlyKey(VK_SHIFT)?3.0:1.0);
    const double factor=dt*speed/std::sqrt(double(squared));
    g_flyPosition[0]+=(w*cp*c-a*q)*factor;
    g_flyPosition[1]+=(w*cp*q+a*c)*factor;
    g_flyPosition[2]+=(w*sp+z)*factor;
}
bool TeleportPlayerToFlyCamera(void* actor,const uint8_t* view){
    if(!actor||!IsReadableRange(actor,sizeof(uintptr_t)))return false;
    double currentRoot[3]{};
    void* checkActor=nullptr;
    if(!GetPlayerRootFromCamera(reinterpret_cast<void*>(g_playerCamera.load()),checkActor,currentRoot)||
       checkActor!=actor)return false;
    // If the game has moved the player while freecam is on, it may be scripted
    // gameplay, a cutscene or a reload. Refuse to teleport a different pawn.
    double drift=0;
    for(int i=0;i<3;++i){
        const double diff=currentRoot[i]-g_flyStartOwner[i];
        drift+=diff*diff;
    }
    if(drift>60.0*60.0)return false;
    double desired[3]{};
    for(int i=0;i<3;++i)desired[i]=g_flyPosition[i]-g_flyCameraOffset[i];
    if(!Valid3(desired))return false;
    double rotation[3]{};
    std::memcpy(rotation,view+0x18,sizeof(rotation));
    if(!Valid3(rotation))return false;
    void* vtable=nullptr;
    std::memcpy(&vtable,actor,sizeof(vtable));
    if(!IsReadableRange(vtable,0x5E0))return false;
    void* function=nullptr;
    std::memcpy(&function,static_cast<const uint8_t*>(vtable)+0x5D8,sizeof(function));
    auto* exe=reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
    const auto fn=reinterpret_cast<uintptr_t>(function);
    const auto base=reinterpret_cast<uintptr_t>(exe);
    if(!exe||fn<base+0x1000||fn>=base+0x749e3dc)return false;
    using TeleportTo=bool(__fastcall*)(void*,const double*,const double*,bool,bool);
    const bool ok=reinterpret_cast<TeleportTo>(function)(actor,desired,rotation,false,false);
    if(ok){
        ++g_teleportSuccess;
        StopFly(L"F3: native actor TeleportTo returned success; freecam disabled.");
    }else{
        ++g_teleportFailed;
        Log(L"F3: native teleport refused target (collision or gameplay constraint).");
    }
    return ok;
}
void HandleFlyCamera(void* camera,uint8_t* view,bool aiming){
    if(!g_flyActive.load())return;
    if(aiming){
        StopFly(L"Freecam cancelled during native ADS/scope.");
        return;
    }
    void* actor=nullptr;
    double root[3]{};
    double originalView[3]{};
    std::memcpy(originalView,view,sizeof(originalView));
    if(!Valid3(originalView)||!GetPlayerRootFromCamera(camera,actor,root)){
        StopFly(L"Freecam cancelled: owner actor or native camera location invalid.");
        return;
    }
    if(g_flyNeedsOrigin.exchange(false)){
        // A separate actor-camera would not have the player's view at its
        // root. Require plausible first-person camera-to-pawn distance.
        const double dx=originalView[0]-root[0];
        const double dy=originalView[1]-root[1];
        const double dz=originalView[2]-root[2];
        if(dx*dx+dy*dy>120.0*120.0||std::abs(dz)>260.0){
            StopFly(L"Freecam refused: camera owner is not sufficiently close to pawn root.");
            return;
        }
        for(int i=0;i<3;++i){
            g_flyPosition[i]=originalView[i];
            g_flyCameraOffset[i]=originalView[i]-root[i];
            g_flyStartOwner[i]=root[i];
        }
        g_flyOwner=reinterpret_cast<uintptr_t>(actor);
        g_flyLastTick=GetTickCount64();
        Log(L"Freecam origin captured and pawn root proximity validated.");
    }
    if(g_flyOwner.load()!=reinterpret_cast<uintptr_t>(actor)){
        StopFly(L"Freecam stopped: different actor or cinematic camera.");
        return;
    }
    double drift=0.0;
    for(int i=0;i<3;++i){
        const double d=root[i]-g_flyStartOwner[i];
        drift+=d*d;
    }
    if(drift>60.0*60.0){
        StopFly(L"Freecam stopped: native player actor moved while freecam was active.");
        return;
    }
    if(g_teleportPending.exchange(false)){
        if(!TeleportPlayerToFlyCamera(actor,view)){
            ++g_teleportFailed;
            Log(L"F3: teleport rejected by owner validation, native position or actor method.");
        }
        return;
    }
    UpdateFlightLocation(view);
    if(!Valid3(g_flyPosition)){
        StopFly(L"Freecam stopped: invalid flight coordinates.");
        return;
    }
    std::memcpy(view,g_flyPosition,sizeof(g_flyPosition));
    ++g_flyFrames;
}


// Automatically discover the first-person player camera after gameplay starts.
// This is a conservative opt-in-by-default observation, not a static pointer
// scan. Only the already-verified GetCameraView callback is considered.
// Cinematics often switch cameras or native FOV, so this never changes those.
void SetGameplayCamera(void* camera,float world,float fps,bool autoDetected){
    g_playerCamera=reinterpret_cast<uintptr_t>(camera);
    g_baselineWorld=world;g_baselineFps=fps;
    g_nativeWorld=world;g_nativeFps=fps;
    g_playerLastSeen=GetTickCount64();
    g_autoCandidate=0;
    g_autoCandidateFrames=0;
    if(autoDetected)++g_autoCameraCaptures;
    StopFly(L"Freecam stopped: gameplay camera acquired or changed.");
    wchar_t line[240]{};
    swprintf_s(line,L"Gameplay camera %s: %p, native World=%.2f, FPS=%.2f.",
        autoDetected?L"AUTO CAPTURED":L"MANUALLY CAPTURED",camera,
        double(world),double(fps));
    Log(line);
}
bool CameraOwnerLooksLikeGameplay(void* camera,const uint8_t* view){
    void* owner=nullptr;
    double root[3]{},location[3]{};
    std::memcpy(location,view,sizeof(location));
    if(!Valid3(location) || !GetPlayerRootFromCamera(camera,owner,root))return false;
    double dx=location[0]-root[0],dy=location[1]-root[1],dz=location[2]-root[2];
    if(dx*dx+dy*dy>120.0*120.0 || std::abs(dz)>260.0)return false;
    return true;
}
void AutoCaptureCamera(void* camera,uint8_t* view,float world,float fps,bool aiming){
    if(!g_autoCamera.load()||g_captureCamera.load()||g_flyActive.load()||
       aiming||g_menuOpen.load()||
       !g_gameHwnd||GetForegroundWindow()!=g_gameHwnd)return;
    const uint64_t now=GetTickCount64();
    const uintptr_t ptr=reinterpret_cast<uintptr_t>(camera);
    const uintptr_t owner=g_playerCamera.load();
    if(owner==ptr){
        g_playerLastSeen=now;
        return;
    }
    // On a level load or after a cinematic camera swap, the previously
    // selected camera can disappear. Recover only when it has not appeared
    // for >=2.5 seconds and the new candidate is stable.
    if(owner){
        const uint64_t seen=g_playerLastSeen.load();
        if(seen&&now-seen<2500)return;
    }
    if(!CameraOwnerLooksLikeGameplay(camera,view)){
        // A cinematic camera can be a real component, but typically isn't
        // in the same first-person actor coordinate neighbourhood.
        if(g_autoCandidate.load()==ptr){
            g_autoCandidate=0;g_autoCandidateFrames=0;
        }
        return;
    }
    const uintptr_t prior=g_autoCandidate.load();
    if(prior!=ptr || std::abs(world-g_candidateWorld.load())>0.25f ||
       std::abs(fps-g_candidateFPS.load())>0.25f) {
        g_autoCandidate=ptr;g_autoCandidateSince=now;
        g_autoCandidateFrames=1;g_candidateWorld=world;g_candidateFPS=fps;
        return;
    }
    ++g_autoCandidateFrames;
    const uint64_t since=g_autoCandidateSince.load();
    if(since && now-since>=1500 && g_autoCandidateFrames.load()>=30){
        if(owner&&owner!=ptr){
            ++g_autoCameraReleases;
            StopFly(L"Freecam stopped: new gameplay camera after transition.");
        }
        SetGameplayCamera(camera,world,fps,true);
    }
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
    if(g_captureCamera.load()&&!aiming){
        // Keep the user-controlled fallback from V0.4A, unchanged in function.
        SetGameplayCamera(camera,baseWorld,baseFPS,false);
        g_captureCamera=false;
    }else{
        AutoCaptureCamera(camera,view,baseWorld,baseFPS,aiming);
    }
    if(g_playerCamera.load()!=ptr)return; // camera swap (cinematics) => vanilla
    g_playerLastSeen=GetTickCount64();
    g_nativeWorld.store(baseWorld);
    g_nativeFps.store(baseFPS);
    // Aiming, scope zoom, camera blends, FOV animation or gameplay cameras
    // outside their captured baseline must remain fully under native control.
    if(aiming ||
       std::abs(baseWorld-g_baselineWorld.load())>1.0f ||
       std::abs(baseFPS-g_baselineFps.load())>1.0f) {
        if(g_flyActive.load())StopFly(L"Freecam disabled by zoom, ADS or native FOV transition.");
        ++g_scopeBypass;
        return;
    }
    HandleFlyCamera(camera,view,aiming);
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
                    if(capture==1){
                        if(int(wp)!=g_cheatShopVK.load()&&int(wp)!=g_flyVK.load()&&
                           int(wp)!=g_teleportVK.load())g_menuVK=int(wp);
                    }
                    if(capture==3){
                        if(int(wp)!=g_menuVK.load()&&int(wp)!=g_flyVK.load()&&
                           int(wp)!=g_teleportVK.load())g_cheatShopVK=int(wp);
                    }
                    if(capture==4&&int(wp)!=g_menuVK.load()&&
                       int(wp)!=g_cheatShopVK.load()&&int(wp)!=g_teleportVK.load())
                        g_flyVK=int(wp);
                    if(capture==5&&int(wp)!=g_menuVK.load()&&
                       int(wp)!=g_cheatShopVK.load()&&int(wp)!=g_flyVK.load())
                        g_teleportVK=int(wp);
                    SaveSettings();
                    Log(L"Hotkey rebound through native game window.");
                }
                return 0;
            }
            if(int(wp)==g_menuVK.load()){
                g_menuOpen=!g_menuOpen.load();
                return 0;
            }
            if(int(wp)==g_cheatShopVK.load()){
                StopFly(L"Freecam stopped before CheatShop.");
                g_menuOpen=false;
                RequestCheatShop();
                return 0;
            }
            if(int(wp)==g_flyVK.load()){
                ToggleFly();
                return 0;
            }
            if(int(wp)==g_teleportVK.load()&&g_flyActive.load()){
                QueueTeleport();
                return 0;
            }
            // Developer-console menu remains out of scope.
        }
    }
    if(g_flyActive.load()){
        // Send only the freecam controls to the overlay; do not let ordinary
        // keyboard movement reach the game's EnhancedInput pipeline.
        if(msg==WM_KEYDOWN||msg==WM_KEYUP||msg==WM_SYSKEYDOWN||msg==WM_SYSKEYUP){
            const int key=int(wp);
            if(key=='W'||key=='A'||key=='S'||key=='D'||key=='Z'||key=='Q'||
               key==VK_UP||key==VK_DOWN||key==VK_LEFT||key==VK_RIGHT||
               key==VK_SPACE||key==VK_CONTROL||key==VK_SHIFT)return 0;
        }
        if(msg==WM_CHAR&&(wp=='w'||wp=='a'||wp=='s'||wp=='d'||
            wp=='z'||wp=='q'||wp=='W'||wp=='A'||wp=='S'||wp=='D'))return 0;
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
    if(ImGui::Begin("Crisol Enhanced PC Mod | V0.8B CHEATSHOP INPUT TEST",
        &open,ImGuiWindowFlags_NoCollapse)){
        ImGui::TextColored(ImVec4(.52f,.83f,1.f,1.f),"CRISOL ENHANCED PC MOD");
        ImGui::SameLine();ImGui::TextDisabled("V0.8B | AUTO FOV + SKIP INTRO");
        ImGui::Separator();ImGui::Spacing();
        ImGui::SetNextItemOpen(true,ImGuiCond_Once);
        if(layout::Group("Camera / Gameplay ownership")){
            if(!g_cameraHookReady.load()){
                ImGui::TextColored(ImVec4(1.f,.72f,.42f,1.f),
                    "Camera hook unavailable; this EXE does not match the verified target.");
            }else if(g_playerCamera.load()==0){
                ImGui::TextColored(ImVec4(1.f,.78f,.40f,1.f),
                    "Waiting for gameplay camera; automatic detection runs after loading.");
            }else{
                ImGui::TextColored(ImVec4(.45f,.90f,.69f,1.f),
                    "Camera captured. Scoped edits are restricted to this instance.");
                ImGui::Text("Original World FOV: %.1f | Original FPS FOV: %.1f",
                    g_baselineWorld.load(),g_baselineFps.load());
            }
            bool automatic=g_autoCamera.load();
            if(ImGui::Checkbox("Auto-detect gameplay camera",&automatic)){
                g_autoCamera=automatic;SaveSettings();
                g_autoCandidate=0;g_autoCandidateFrames=0;
            }
            ImGui::TextDisabled("Captures a stable, first-person camera after loading; native zoom/cinematics keep priority.");
            ImGui::Text("Auto captures: %u | transitions: %u",
                g_autoCameraCaptures.load(),g_autoCameraReleases.load());
            ImGui::TextWrapped("Manual recapture remains available if the game camera is not recognized.");
            if(ImGui::Button(g_captureCamera.load()?"Waiting for active camera...":"Recapture gameplay camera (manual)")){
                StopFly(L"Freecam ended by manual camera recapture.");
                g_captureCamera=true;
                g_playerCamera=0;
            }
            ImGui::SameLine();
            if(ImGui::Button("Release")){
                StopFly(L"Freecam ended by camera Release.");
                g_playerCamera=0;g_captureCamera=false;
            }
            ImGui::Spacing();
        }
        ImGui::SetNextItemOpen(true,ImGuiCond_Once);
        if(layout::Group("Freecam / Teleport to camera")){
            ImGui::TextWrapped("F2: detach the view. Fly to a clear location, then F3: teleport the player there and restore normal camera.");
            ImGui::TextDisabled("WASD / ZQSD, Space up, Ctrl down, Shift fast. Choose an empty landing area.");
            ImGui::TextDisabled("Teleportation uses the game's collision-tested AActor::TeleportTo method.");
            if(!g_playerCamera.load()){
                ImGui::TextColored(ImVec4(1.f,.78f,.45f,1.f),
                    "Capture gameplay camera first, outside cutscenes and zoom.");
            }
            if(ImGui::Button(g_flyActive.load()?"F2: Exit freecam":"F2: Enable freecam"))
                ToggleFly();
            ImGui::SameLine();
            ImGui::BeginDisabled(!g_flyActive.load());
            if(ImGui::Button("F3: Teleport and return")){
                g_menuOpen=false;open=false;QueueTeleport();
            }
            ImGui::EndDisabled();
            ImGui::Text("State: %s",g_flyActive.load()?"FREECAM":"normal gameplay");
            int speed=g_flySpeed.load();
            if(ImGui::SliderInt("Fly speed",&speed,100,2000)){
                g_flySpeed=speed;SaveSettings();
            }
            ImGui::Text("Freecam key: VK %d | Teleport key: VK %d",
                g_flyVK.load(),g_teleportVK.load());
            if(ImGui::Button(g_captureHotkey.load()==4?
                "Press freecam key...":"Rebind F2 / Freecam"))g_captureHotkey=4;
            ImGui::SameLine();
            if(ImGui::Button(g_captureHotkey.load()==5?
                "Press teleport key...":"Rebind F3 / Teleport"))g_captureHotkey=5;
            ImGui::TextDisabled("If the game moves the player or zooms, freecam/teleport is safely cancelled.");
            ImGui::Text("Flight frames: %u | teleports: %u succeeded, %u refused",
                g_flyFrames.load(),g_teleportSuccess.load(),g_teleportFailed.load());
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
            if(ImGui::Button("Restore vanilla settings")){
                g_worldEnabled=false;g_fpsEnabled=false;
                g_worldFov=90;g_fpsFov=85;
                SaveSettings();
            }
        }
        ImGui::SetNextItemOpen(true,ImGuiCond_Once);
        if(layout::Group("CheatShop (SML external mod)")){
            ImGui::TextWrapped("Launch SML's 'cheatshop' command with one key press. Press again to close its shop.");
            ImGui::TextDisabled("Requires SML. Start with its console closed; the console may flash briefly.");
            if(ImGui::Button("Open / close CheatShop")){
                g_menuOpen=false;
                open=false;
                RequestCheatShop();
            }
            ImGui::Spacing();
            const int vk=g_cheatShopVK.load();
            if(vk>=VK_F1&&vk<=VK_F12)
                ImGui::Text("CheatShop shortcut: F%d",vk-VK_F1+1);
            else
                ImGui::Text("CheatShop shortcut: Virtual key %d",vk);
            if(ImGui::Button(g_captureHotkey.load()==3?
                    "Press a key (Esc to cancel)##shop":"Change CheatShop key"))
                g_captureHotkey=3;
            ImGui::SameLine();
            if(ImGui::Button("Reset to F1")){
                g_cheatShopVK=VK_F1;g_captureHotkey=0;SaveSettings();
            }
            ImGui::Spacing();
            ImGui::TextDisabled("If SML uses a custom console key, set SMLConsoleVirtualKey in INI.");
            ImGui::Text("Console opening key: %s",g_smlConsoleVK.load()?"INI VK override":
                SMLLaunchConsoleVK()?"SML launch -consolekey=F-key":"` / ~ (Tilde)");
            ImGui::Text("Sequences sent: %u / requests: %u",
                g_cheatShopSequences.load(),g_cheatShopAttempts.load());
        }
        ImGui::SetNextItemOpen(true,ImGuiCond_Once);
        if(layout::Group("Startup / Skip Intro")){
            bool skip=g_skipStartupVideos.load();
            if(ImGui::Checkbox("Skip startup logo videos",&skip)){
                g_skipStartupVideos=skip;SaveSettings();
            }
            ImGui::TextDisabled("Targets BlumLogo, AnimacionLogoNew and SplashVideo1 MP4 only.");
            ImGui::TextDisabled("No files renamed, replaced or removed; story cinematics remain untouched.");
            ImGui::Text("File hooks: CreateFileW %s / CreateFile2 %s",
                g_introHookWReady.load()?"ready":"unavailable",
                g_introHook2Ready.load()?"ready":"unavailable");
            ImGui::Text("Startup movie open requests blocked: %u",
                g_introVideoOpensSuppressed.load());
            ImGui::TextDisabled("Toggle is saved; restart the game to test startup skipping.");
            ImGui::TextDisabled("If blocked = 0, the game's startup movie loading bypassed our hooks.");
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
            ImGui::TextDisabled("Camera auto-detection is enabled by default (manual fallback retained).");
            ImGui::TextDisabled("Cinema using same player camera needs in-game validation.");
            ImGui::TextDisabled("No global FOV scalar patch; no embedded developer console.");
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
            InstallIntroVideoHooks();
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
