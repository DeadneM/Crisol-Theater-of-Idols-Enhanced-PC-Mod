#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <Unknwn.h>
#include <atomic>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

namespace {
HMODULE g_self=nullptr, g_dxgi=nullptr;
std::once_flag g_realOnce, g_loaderOnce;
std::vector<HMODULE> g_plugins;
std::wstring g_folder;
std::wstring g_logPath;

void Log(const wchar_t* message) {
    if (g_logPath.empty()) return;
    HANDLE file=CreateFileW(g_logPath.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE) return;
    SYSTEMTIME st{}; GetLocalTime(&st);
    wchar_t timestamp[40]{};
    wsprintfW(timestamp,L"[%02u:%02u:%02u] ",st.wHour,st.wMinute,st.wSecond);
    DWORD written{};
    WriteFile(file,timestamp,static_cast<DWORD>(wcslen(timestamp)*sizeof(wchar_t)),&written,nullptr);
    WriteFile(file,message,static_cast<DWORD>(wcslen(message)*sizeof(wchar_t)),&written,nullptr);
    static const wchar_t nl[]=L"\r\n";
    WriteFile(file,nl,sizeof(nl)-sizeof(wchar_t),&written,nullptr);
    CloseHandle(file);
}
void LoadReal() {
    std::call_once(g_realOnce,[] {
        wchar_t system[MAX_PATH]{};
        UINT n=GetSystemDirectoryW(system,MAX_PATH);
        if(n && n<MAX_PATH-11) {
            std::wstring path(system);
            path+=L"\\dxgi.dll";
            g_dxgi=LoadLibraryW(path.c_str());
        }
    });
}
FARPROC Real(const char* name) {
    LoadReal();
    return g_dxgi?GetProcAddress(g_dxgi,name):nullptr;
}
void LoadAsi() {
    std::call_once(g_loaderOnce,[] {
        wchar_t path[MAX_PATH]{};
        DWORD n=GetModuleFileNameW(g_self,path,MAX_PATH);
        if(!n || n==MAX_PATH) return;
        wchar_t* slash=wcsrchr(path,L'\\');
        if(!slash) return;
        slash[1]=0;
        g_folder=path;
        g_logPath=g_folder+L"CRToiLoader.log";
        Log(L"DXGI proxy initialized. Scanning local ASI files.");
        WIN32_FIND_DATAW fd{};
        HANDLE search=FindFirstFileW((g_folder+L"*.asi").c_str(),&fd);
        if(search==INVALID_HANDLE_VALUE) {Log(L"No .asi files in EXE folder");return;}
        do {
            if(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY) continue;
            HMODULE asi=LoadLibraryW((g_folder+fd.cFileName).c_str());
            if(!asi) {Log(L"ASI LoadLibraryW failed");continue;}
            g_plugins.push_back(asi);
            Log(L"ASI loaded");
            using Init=void(WINAPI*)();
            auto init=reinterpret_cast<Init>(GetProcAddress(asi,"CRToi_Initialize"));
            if(init) init();
        }while(FindNextFileW(search,&fd));
        FindClose(search);
    });
}
void NotifyFactory(void* factory) {
    if(!factory) return;
    LoadAsi();
    using Notify=void(WINAPI*)(IUnknown*);
    for(HMODULE module:g_plugins) {
        auto callback=reinterpret_cast<Notify>(GetProcAddress(module,"CRToi_OnFactory"));
        if(callback)callback(static_cast<IUnknown*>(factory));
    }
}
} // namespace

extern "C" __declspec(dllexport)
HRESULT WINAPI CreateDXGIFactory(REFIID riid,void** out) {
    using Fn=HRESULT(WINAPI*)(REFIID,void**);
    auto fn=reinterpret_cast<Fn>(Real("CreateDXGIFactory"));
    if(!fn)return E_FAIL;
    HRESULT hr=fn(riid,out);
    if(SUCCEEDED(hr)&&out)NotifyFactory(*out);
    return hr;
}
extern "C" __declspec(dllexport)
HRESULT WINAPI CreateDXGIFactory1(REFIID riid,void** out) {
    using Fn=HRESULT(WINAPI*)(REFIID,void**);
    auto fn=reinterpret_cast<Fn>(Real("CreateDXGIFactory1"));
    if(!fn)return E_FAIL;
    HRESULT hr=fn(riid,out);
    if(SUCCEEDED(hr)&&out)NotifyFactory(*out);
    return hr;
}
extern "C" __declspec(dllexport)
HRESULT WINAPI CreateDXGIFactory2(UINT flags,REFIID riid,void** out) {
    using Fn=HRESULT(WINAPI*)(UINT,REFIID,void**);
    auto fn=reinterpret_cast<Fn>(Real("CreateDXGIFactory2"));
    if(!fn)return E_FAIL;
    HRESULT hr=fn(flags,riid,out);
    if(SUCCEEDED(hr)&&out)NotifyFactory(*out);
    return hr;
}
extern "C" __declspec(dllexport)
HRESULT WINAPI DXGIGetDebugInterface1(UINT flags,REFIID riid,void** out) {
    using Fn=HRESULT(WINAPI*)(UINT,REFIID,void**);
    auto fn=reinterpret_cast<Fn>(Real("DXGIGetDebugInterface1"));
    return fn?fn(flags,riid,out):E_NOINTERFACE;
}
extern "C" __declspec(dllexport)
HRESULT WINAPI DXGIGetDebugInterface(REFIID riid,void** out) {
    using Fn=HRESULT(WINAPI*)(REFIID,void**);
    auto fn=reinterpret_cast<Fn>(Real("DXGIGetDebugInterface"));
    return fn?fn(riid,out):E_NOINTERFACE;
}
extern "C" __declspec(dllexport)
HRESULT WINAPI DXGIDeclareAdapterRemovalSupport() {
    using Fn=HRESULT(WINAPI*)();
    auto fn=reinterpret_cast<Fn>(Real("DXGIDeclareAdapterRemovalSupport"));
    return fn?fn():E_NOTIMPL;
}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,void*) {
    if(reason==DLL_PROCESS_ATTACH) {
        g_self=module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
