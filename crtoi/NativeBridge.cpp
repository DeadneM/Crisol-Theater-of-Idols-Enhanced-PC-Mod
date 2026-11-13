#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include "NativeBridge.h"
#include <array>
#include <atomic>
#include <cstring>
#include <iterator>
#include <limits>
#include <cstdio>
#include <cstdint>
#include <cstddef>
#include <shlobj.h>
#include <string>

namespace crtoi::nativebridge {
namespace {
Logger logger=nullptr;
uintptr_t image=0;
std::atomic_bool quickSupported{false},ironSupported{false},ironEnabled{false};
std::atomic<void*> ironOwner{nullptr};
std::atomic_bool ironAiming{false};
std::atomic_uint64_t ironSeenAt{0};
void* forcedManager=nullptr;void* forcedWeapon=nullptr;

std::atomic_uint64_t quickAt{0},playableAt{0},quickStatusAt{0};
std::atomic<void*> playableOwner{nullptr};
std::atomic<HWND> quickWindow{nullptr};
enum class QuickStatus {Idle,Queued,Unavailable,BackupFailed,Requested,Written,NoWrite,Busy};
std::atomic<QuickStatus> quickStatus{QuickStatus::Idle};
std::wstring autoSavePath;
WIN32_FILE_ATTRIBUTE_DATA beforeSave{};
uint64_t checkingSaveAt=0,lastQuickAttempt=0;
void SetQuickStatus(QuickStatus value,const wchar_t* message){
    quickStatus=value;quickStatusAt=GetTickCount64();
    if(logger&&message)logger(message);
}

constexpr uintptr_t kNamePool=0x96E8600;
std::atomic_bool supported{false},consoleSupported{false};
std::atomic_uint64_t pendingAt{0};
std::atomic<HWND> requestedWindow{nullptr};
std::atomic_uint requests{0},dispatches{0},cancellations{0};
using WorldTickFn=void(__fastcall*)(void*,int,float);
WorldTickFn originalTick=nullptr;
using EventFn=void(__fastcall*)(void*,void*,void*);
EventFn originalEvent=nullptr;
std::atomic_bool closeSupported{false},weaponSupported{false};
void* shopWidget=nullptr;void* shopGraph=nullptr;

std::atomic<void*> lastTickWorld{nullptr};
std::atomic_uint64_t lastTickAt{0};
std::atomic_bool lastTickPlayable{false};
bool Range(const void* ptr,size_t size,bool write=false) {
    if(!ptr||!size)return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if(VirtualQuery(ptr,&mbi,sizeof(mbi))!=sizeof(mbi)||mbi.State!=MEM_COMMIT||
       (mbi.Protect&(PAGE_NOACCESS|PAGE_GUARD)))return false;
    const DWORD p=mbi.Protect&0xff;
    const bool writable=p==PAGE_READWRITE||p==PAGE_WRITECOPY||
        p==PAGE_EXECUTE_READWRITE||p==PAGE_EXECUTE_WRITECOPY;
    if(write&&!writable)return false;
    if(!write&&!writable&&p!=PAGE_READONLY&&p!=PAGE_EXECUTE_READ)return false;
    const uintptr_t at=reinterpret_cast<uintptr_t>(ptr);
    const uintptr_t begin=reinterpret_cast<uintptr_t>(mbi.BaseAddress);
    return at>=begin&&at-begin<=mbi.RegionSize&&size<=mbi.RegionSize-(at-begin);
}

template<class T> bool Read(const void* base,size_t offset,T& value) {
    if(!base||offset>std::numeric_limits<uintptr_t>::max()-reinterpret_cast<uintptr_t>(base))return false;
    const void* p=reinterpret_cast<const void*>(reinterpret_cast<uintptr_t>(base)+offset);
    if(!Range(p,sizeof(T)))return false;
    std::memcpy(&value,p,sizeof(T));return true;
}

bool NameEquals(uint32_t index,const char* expected,bool prefix=false) {
    const size_t length=std::strlen(expected);
    if(length>127||index>>16>=8192)return false;
    const void* pool=reinterpret_cast<const void*>(image+kNamePool);
    void* block=nullptr;
    if(!Read(pool,0x10+size_t(index>>16)*8,block)||!block)return false;
    const size_t offset=size_t(index&0xffff)*2;
    if(reinterpret_cast<uintptr_t>(block)>std::numeric_limits<uintptr_t>::max()-offset)return false;
    const auto* entry=reinterpret_cast<const uint8_t*>(reinterpret_cast<uintptr_t>(block)+offset);
    uint16_t header=0;
    if(!Read(entry,0,header)||(!prefix&&size_t(header>>6)!=length)||
       (prefix&&size_t(header>>6)<length))return false;
    if(header&1) {
        if(!Range(entry+2,length*2))return false;
        for(size_t i=0;i<length;++i) {
            uint16_t c=0;std::memcpy(&c,entry+2+i*2,2);
            if(c!=static_cast<unsigned char>(expected[i]))return false;
        }
        return true;
    }
    return Range(entry+2,length)&&std::memcmp(entry+2,expected,length)==0;
}

bool Named(const void* ptr,size_t offset,const char* expected) {
    uint32_t id=0,number=0;
    return Read(ptr,offset,id)&&Read(ptr,offset+4,number)&&!number&&NameEquals(id,expected);
}

struct Property {void* address=nullptr;int32_t offset=0;uint8_t byteOffset=0,byteMask=0,fieldMask=0;};
bool FindProperty(void* structure,const char* name,const char* type,int32_t size,
                  Property& out,bool parents) {
    for(int depth=0;structure&&depth<(parents?8:1);++depth) {
        void* property=nullptr;int32_t total=0;
        if(!Read(structure,0x50,property)||!Read(structure,0x58,total)||total<=0||total>0x100000)return false;
        for(unsigned n=0;property&&n<256;++n) {
            if(Named(property,0x20,name)) {
                int32_t dim=0,element=0,offset=0;void* fieldClass=nullptr;
                if(!Read(property,0x30,dim)||!Read(property,0x34,element)||
                   !Read(property,0x44,offset)||!Read(property,8,fieldClass)||
                   dim!=1||element!=size||offset<0||offset>total-size||
                   !Named(fieldClass,0,type))return false;
                out.address=property;out.offset=offset;
                if(std::strcmp(type,"BoolProperty")==0) {
                    if(!Read(property,0x71,out.byteOffset)||!Read(property,0x72,out.byteMask)||
                       !Read(property,0x73,out.fieldMask)||out.byteOffset>=size||
                       !out.byteMask||!out.fieldMask||(out.byteMask&out.fieldMask)!=out.byteMask)return false;
                }
                return true;
            }
            if(!Read(property,0x18,property))return false;
        }
        if(!parents||!Read(structure,0x40,structure))return false;
    }
    return false;
}

bool HashMatches() {
    wchar_t path[32768]{};
    const DWORD n=GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));
    if(!n||n>=std::size(path))return false;
    HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0;
    if(ok)ok=BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0;
    std::array<uint8_t,65536> buffer{};DWORD got=0;
    while(ok) {
        if(!ReadFile(file,buffer.data(),DWORD(buffer.size()),&got,nullptr)) {ok=false;break;}
        if(!got)break;
        ok=BCryptHashData(hash,buffer.data(),got,0)>=0;
    }
    std::array<uint8_t,32> digest{};
    if(ok)ok=BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)>=0;
    if(hash)BCryptDestroyHash(hash);
    if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    CloseHandle(file);
    constexpr std::array<uint8_t,32> expected={0xb8,0x42,0x22,0x59,0xfd,0xe6,0xaa,0xae,0x65,0xf2,0x07,0x5c,0x9d,0x26,0xa6,0x0a,0xbf,0xb1,0xac,0x4c,0x0f,0x23,0x12,0x17,0xd4,0x58,0xbf,0xe2,0x84,0x18,0xd3,0x78};
    return ok&&digest==expected;
}

bool OnGameThread() {
    DWORD id=0;uint8_t initialized=0;
    return Read(reinterpret_cast<void*>(image),0x96DD2E8,id)&&id&&
        Read(reinterpret_cast<void*>(image),0x96DD2F4,initialized)&&initialized&&GetCurrentThreadId()==id;
}
bool DerivedFrom(void* object,const char* className) {
    void* cls=nullptr;
    if(!Read(object,0x10,cls))return false;
    for(int n=0;cls&&n<16;++n) {
        if(Named(cls,0x18,className))return true;
        if(!Read(cls,0x40,cls))return false;
    }
    return false;
}
bool LiveObject(void* object) {
    uint32_t flags=0;
    return Read(object,8,flags)&&(flags&(0x8000u|0x10000u))==0;
}
struct FieldCache {void* cls=nullptr;Property property{};bool valid=false;};
bool CachedProperty(FieldCache& cache,void* cls,const char* name,const char* type,int size,Property& out) {
    if(cache.cls!=cls) {
        cache.cls=cls;cache.property={};
        cache.valid=FindProperty(cls,name,type,size,cache.property,true);
    }
    if(!cache.valid||!Named(cache.property.address,0x20,name))return false;
    out=cache.property;return true;
}
bool ControllerCinematic(void* controller,bool& value) {
    thread_local FieldCache cache;
    void* cls=nullptr;Property p;uint8_t raw=0;
    if(!Read(controller,0x10,cls)||!CachedProperty(cache,cls,"bCinematicMode","BoolProperty",1,p)||
       !Read(controller,size_t(p.offset)+p.byteOffset,raw))return false;
    value=(raw&p.fieldMask)!=0;return true;
}
bool CameraController(void* owner,void*& controller) {
    thread_local FieldCache cache;
    void* cls=nullptr;Property p;
    return LiveObject(owner)&&Read(owner,0x10,cls)&&
        CachedProperty(cache,cls,"Controller","ObjectProperty",8,p)&&
        Read(owner,size_t(p.offset),controller)&&LiveObject(controller)&&DerivedFrom(controller,"PlayerController");
}
bool ControllerCursor(void* controller,bool& visible) {
    thread_local FieldCache cache;void* cls=nullptr;Property p;uint8_t raw=0;
    if(!Read(controller,0x10,cls)||!CachedProperty(cache,cls,"bShowMouseCursor","BoolProperty",1,p)||
       !Read(controller,size_t(p.offset)+p.byteOffset,raw))return false;
    visible=(raw&p.fieldMask)!=0;return true;
}
struct FString {wchar_t* data;int32_t count,capacity;};
static_assert(sizeof(FString)==16&&offsetof(FString,count)==8);
using FirstControllerFn=void*(__fastcall*)(void*);
using WorldFromContextFn=void*(__fastcall*)(void*,const void*,int);
using ViewTargetFn=void*(__fastcall*)(void*);
using ConsoleFn=FString*(__fastcall*)(void*,FString*,const FString*,bool);
using FreeFn=void(__fastcall*)(void*);
bool ValidShopGraph(void* widget,void* function) {
    if(!LiveObject(widget)||!DerivedFrom(widget,"ShopCheater_WBP_C")||!LiveObject(function)||
       !Named(function,0x18,"ExecuteUbergraph_ShopCheater_WBP"))return false;
    uint16_t params=0;void* script=nullptr;int32_t length=0;uint8_t entry=0,remove=0;
    Property parameter;
    return Read(function,0xB6,params)&&params==4&&
        FindProperty(function,"EntryPoint","IntProperty",4,parameter,false)&&parameter.offset==0&&
        Read(function,0x60,script)&&Read(function,0x68,length)&&length==1292&&
        Read(script,10,entry)&&entry==0x5f&&Read(script,0x9f,remove)&&remove==0x1b;
}
void __fastcall HookEvent(void* object,void* function,void* params) {
    // Construct's script is EX_LocalFinalFunction followed by the exact graph
    // pointer. Observe that existing pointer instead of searching global objects.
    if(closeSupported.load()&&OnGameThread()&&Named(function,0x18,"Construct")&&
       LiveObject(object)&&DerivedFrom(object,"ShopCheater_WBP_C")) {
        void* script=nullptr;void* graph=nullptr;int32_t length=0;uint8_t opcode=0;
        if(Read(function,0x60,script)&&Read(function,0x68,length)&&length==18&&
           Read(script,0,opcode)&&opcode==0x46&&Read(script,1,graph)&&ValidShopGraph(object,graph)) {
            shopWidget=object;shopGraph=graph;
            if(logger)logger(L"CheatShop: Construct's native close graph observed and checked.");
        }
    }
    if(originalEvent)originalEvent(object,function,params);
}
bool ShopInViewport(void* widget) {
    uint8_t flags=0;void* table=nullptr;void* getWorld=nullptr;
    if(!LiveObject(widget)||!Read(widget,0xd9,flags)||(flags&0x20)==0||
       !Read(widget,0,table)||!Read(table,0x198,getWorld))return false;
    const uintptr_t fn=reinterpret_cast<uintptr_t>(getWorld);
    if(fn<image+0x1000||fn>=image+0x7490000||!Range(getWorld,16))return false;
    // Replicates execIsInViewport 03279335..0327935A without calling an exec/FFrame wrapper.
    void* world=reinterpret_cast<ViewTargetFn>(getWorld)(widget);
    if(!LiveObject(world))return false;
    using SubsystemFn=void*(__fastcall*)(void*);
    using ContainsFn=bool(__fastcall*)(void*,void*);
    void* subsystem=reinterpret_cast<SubsystemFn>(image+0x31DA140)(world);
    return LiveObject(subsystem)&&reinterpret_cast<ContainsFn>(image+0x31E4600)(subsystem,widget);
}
bool CloseShop() {
    if(!closeSupported.load()||!OnGameThread()||!originalEvent||
       !ValidShopGraph(shopWidget,shopGraph)||!ShopInViewport(shopWidget))return false;
    // The real CloseCheaterButton event jumps to VM offset 10. Its graph runs
    // SetInputMode_GameOnly, cursor=false, unpause, RemoveFromParent.
    int32_t entryPoint=10;
    originalEvent(shopWidget,shopGraph,&entryPoint);
    if(logger)logger(L"CheatShop: native Close button graph dispatched. Player-control restoration requires game confirmation.");
    return true;
}
bool ExecuteShop(void* controller) {
    if(!supported.load()||!OnGameThread()||!LiveObject(controller)||!DerivedFrom(controller,"PlayerController"))return false;
    void* table=nullptr;void* target=nullptr;
    if(!Read(controller,0,table)||!Read(table,0xDE0,target))return false;
    const uintptr_t address=reinterpret_cast<uintptr_t>(target);
    if(address<image+0x1000||address>=image+0x7490000||!Range(target,16))return false;
    // Exact dispatch ABI verified in execExecuteConsoleCommand at 039E162E:
    // RCX=controller, RDX=FString return buffer, R8=command, R9b=write-to-log.
    wchar_t text[]=L"cheatshop";
    const FString command{text,int32_t(std::size(text)),int32_t(std::size(text))};
    FString result{};
    reinterpret_cast<ConsoleFn>(target)(controller,&result,&command,true);
    ++dispatches;
    if(logger)logger(L"CheatShop: native PlayerController ConsoleCommand returned on verified game thread; menu response requires user validation.");
    if(result.data&&result.data!=text) {
        if(result.count>0&&result.count<=1024&&result.capacity>=result.count&&
           Range(result.data,size_t(result.count)*sizeof(wchar_t))) {
            if(logger) {
                wchar_t line[320]{};
                swprintf_s(line,L"CheatShop native response: %.*ls",result.count>200?200:result.count-1,result.data);
                logger(line);
            }
        }
        // This is the same one-pointer engine allocator free used for the
        // ConsoleCommand result by the verified exec wrapper at 039E163E.
        reinterpret_cast<FreeFn>(image+0x112ABC0)(result.data);
    }
    return true;
}
void Service(void* world) {
    uint64_t requested=pendingAt.load();
    if(!requested)return;
    const uint64_t now=GetTickCount64();
    if(now-requested>10000) {
        if(pendingAt.compare_exchange_strong(requested,0)) {
            ++cancellations;
            if(logger)logger(L"CheatShop: request expired; no valid game-thread controller was available. No command executed.");
        }
        return;
    }
    if(GetForegroundWindow()!=requestedWindow.load()) {
        if(pendingAt.compare_exchange_strong(requested,0)) {
            ++cancellations;
            if(logger)logger(L"CheatShop: request cancelled after game lost focus. No command executed.");
        }
        return;
    }
    if(!OnGameThread()) {
        static std::atomic_bool logged{false};
        if(!logged.exchange(true)&&logger) {
            DWORD expected=0;Read(reinterpret_cast<void*>(image),0x96DD2E8,expected);
            wchar_t line[160]{};swprintf_s(line,L"Native console deferred: current thread=%lu; engine game thread=%lu.",GetCurrentThreadId(),expected);logger(line);
        }
        return;
    }
    if(!LiveObject(world)||!DerivedFrom(world,"World"))return;
    // UWorld::GetFirstPlayerController, as used by the verified console wrapper
    // at 039E160E. World-owned weak-controller resolution stays native.
    void* controller=reinterpret_cast<FirstControllerFn>(image+0x3FA9D70)(world);
    if(!LiveObject(controller)||!DerivedFrom(controller,"PlayerController"))return;
    static uint64_t lastDispatch=0;
    // Retain the queued press through debounce instead of silently dropping it.
    if(lastDispatch&&now-lastDispatch<750)return;
    if(!pendingAt.compare_exchange_strong(requested,0))return;
    if(closeSupported.load()&&ValidShopGraph(shopWidget,shopGraph)&&ShopInViewport(shopWidget)) {
        lastDispatch=now;
        if(!CloseShop())++cancellations;
        return;
    }
    bool cursor=false;
    if(!ControllerCursor(controller,cursor)||cursor) {
        ++cancellations;
        if(logger)logger(L"CheatShop: no verified visible shop to close; no command or input reset executed.");
        return;
    }
    lastDispatch=now;
    if(!ExecuteShop(controller)) {
        ++cancellations;
        if(logger)logger(L"CheatShop: controller or native dispatch checks failed. No keyboard fallback used.");
    }
}
bool SameCopy(const std::wstring& a,const std::wstring& b) {
    HANDLE first=CreateFileW(a.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(first==INVALID_HANDLE_VALUE)return false;
    HANDLE second=CreateFileW(b.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(second==INVALID_HANDLE_VALUE){CloseHandle(first);return false;}
    std::array<uint8_t,65536> x{},y{};bool ok=true;DWORD nx=0,ny=0;
    do {
        ok=ReadFile(first,x.data(),DWORD(x.size()),&nx,nullptr)&&
           ReadFile(second,y.data(),DWORD(y.size()),&ny,nullptr)&&nx==ny&&std::memcmp(x.data(),y.data(),nx)==0;
    }while(ok&&nx);
    CloseHandle(first);CloseHandle(second);return ok;
}
bool BackupAutoSave() {
    PWSTR local=nullptr;
    if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))return false;
    std::wstring saved=std::wstring(local)+L"\\CRToiPrototype\\Saved";CoTaskMemFree(local);
    autoSavePath=saved+L"\\SaveGames\\AutoSave.sav";
    beforeSave={};
    if(!GetFileAttributesExW(autoSavePath.c_str(),GetFileExInfoStandard,&beforeSave))
        return GetLastError()==ERROR_FILE_NOT_FOUND;
    const std::wstring root=saved+L"\\CRToiEnhancedBackups";
    if(!CreateDirectoryW(root.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
    SYSTEMTIME time{};GetLocalTime(&time);wchar_t name[100]{};
    swprintf_s(name,L"\\AutoSave_before_F4_%04u%02u%02u_%02u%02u%02u_%03u.sav",
        time.wYear,time.wMonth,time.wDay,time.wHour,time.wMinute,time.wSecond,time.wMilliseconds);
    const std::wstring destination=root+name;
    if(!CopyFileW(autoSavePath.c_str(),destination.c_str(),TRUE)||!SameCopy(autoSavePath,destination))return false;
    if(logger)logger((L"F4 previous autosave copied and byte-verified: "+destination).c_str());
    return true;
}
void ServiceQuickSave(void* world) {
    const uint64_t now=GetTickCount64();
    if(checkingSaveAt) {
        WIN32_FILE_ATTRIBUTE_DATA after{};
        if(GetFileAttributesExW(autoSavePath.c_str(),GetFileExInfoStandard,&after)&&
           (after.nFileSizeHigh||after.nFileSizeLow>64)&&
           (CompareFileTime(&after.ftLastWriteTime,&beforeSave.ftLastWriteTime)!=0||
            after.nFileSizeLow!=beforeSave.nFileSizeLow||after.nFileSizeHigh!=beforeSave.nFileSizeHigh)) {
            checkingSaveAt=0;SetQuickStatus(QuickStatus::Written,L"F4: native AutoSave file updated. Reload validation is still required.");
        }else if(now-checkingSaveAt>10000){checkingSaveAt=0;SetQuickStatus(QuickStatus::NoWrite,L"F4: no AutoSave file update detected; game may have refused the native request.");}
    }
    uint64_t requested=quickAt.load();if(!requested)return;
    if(!quickAt.compare_exchange_strong(requested,0))return;
    if(checkingSaveAt||(lastQuickAttempt&&now-lastQuickAttempt<3000)){
        SetQuickStatus(QuickStatus::Busy,L"F4: save already pending or cooldown active.");return;
    }
    void* owner=playableOwner.load();void* controller=nullptr;
    bool cursor=true;
    if(now-requested>1500||now-playableAt.load()>500||!LiveObject(owner)||
       GetForegroundWindow()!=quickWindow.load()||!LiveObject(world)||!OnGameThread()){
        SetQuickStatus(QuickStatus::Unavailable,L"F4 refused: no recent active captured gameplay camera.");return;
    }
    controller=reinterpret_cast<FirstControllerFn>(image+0x3FA9D70)(world);
    if(!LiveObject(controller)){
        SetQuickStatus(QuickStatus::Unavailable,L"F4 refused: local controller unavailable.");return;
    }
    if(!ControllerCursor(controller,cursor)){
        SetQuickStatus(QuickStatus::Unavailable,L"F4 refused: controller cursor schema unavailable.");return;
    }
    if(cursor){
        SetQuickStatus(QuickStatus::Unavailable,L"F4 refused: a cursor/menu is active.");return;
    }
    // bCinematicMode proved too broad in user gameplay. Do not use it as an
    // absolute veto; the working camera heartbeat and native AutoSave guards apply.
    using ManagerFn=void*(__fastcall*)(const void*);
    using AutoSaveFn=void(__fastcall*)(void*);
    void* manager=reinterpret_cast<ManagerFn>(image+0x4DD2FF0)(world);
    void* context=nullptr;
    if(!LiveObject(manager)||!Read(manager,0x28,context)||!LiveObject(context)){
        SetQuickStatus(QuickStatus::Unavailable,L"F4 refused: native save manager unavailable.");return;
    }
    if(!BackupAutoSave()){
        SetQuickStatus(QuickStatus::BackupFailed,L"F4 cancelled: previous autosave backup could not be verified. No native save called.");return;
    }
    lastQuickAttempt=now;
    SetQuickStatus(QuickStatus::Requested,L"F4: invoking the native AutoSave path (slot 0); manual slots are not targeted.");
    reinterpret_cast<AutoSaveFn>(image+0x4D87040)(manager);
    checkingSaveAt=GetTickCount64();
}
bool IronContext(void* manager,void*& weapon,uint8_t& full) {
    void* animation=nullptr;void* owner=nullptr;uint8_t force=0;
    return LiveObject(manager)&&Read(manager,0xd8,weapon)&&LiveObject(weapon)&&
        DerivedFrom(weapon,"CRWeaponFirePlayerBase")&&
        !DerivedFrom(weapon,"CRDivineSniper_BP_C")&&
        Read(manager,0x100,animation)&&LiveObject(animation)&&
        Read(manager,0xe8,owner)&&LiveObject(owner)&&
        Read(manager,0x190,full)&&full<=1&&Read(manager,0x191,force)&&force==0;
}
void ReleaseIronSight(){
    if(!forcedManager)return;
    void* current=nullptr;uint8_t full=0;
    if(IronContext(forcedManager,current,full)&&current==forcedWeapon&&full){
        using SetFullFn=void(__fastcall*)(void*,bool);
        reinterpret_cast<SetFullFn>(image+0x4D68EF0)(forcedManager,false);
        if(logger)logger(L"Iron-sight TEST: native full-aim release dispatched.");
    }
    forcedManager=nullptr;forcedWeapon=nullptr;
}
void ServiceIronSights(){
    if(!ironSupported.load()||!OnGameThread())return;
    const bool requested=ironEnabled.load()&&ironAiming.load()&&GetTickCount64()-ironSeenAt.load()<300;
    if(!requested){ReleaseIronSight();return;}
    void* owner=ironOwner.load();void* manager=nullptr;void* weapon=nullptr;uint8_t full=0;
    if(!LiveObject(owner)||!Read(owner,0xB60,manager)||!IronContext(manager,weapon,full)){
        ReleaseIronSight();return;
    }
    if(forcedManager&&(forcedManager!=manager||forcedWeapon!=weapon))ReleaseIronSight();
    if(forcedManager||full)return;
    using SetFullFn=void(__fastcall*)(void*,bool);
    reinterpret_cast<SetFullFn>(image+0x4D68EF0)(manager,true);
    uint8_t after=0;
    if(Read(manager,0x190,after)&&after==1){
        forcedManager=manager;forcedWeapon=weapon;
        if(logger)logger(L"Iron-sight TEST: native full-aim transition requested for non-sniper. Actual sight alignment still requires visual verification.");
    }
}
void __fastcall HookWorldTick(void* world,int tickType,float delta) {
    thread_local bool busy=false;
    const bool outer=!busy;busy=true;
    lastTickWorld=world;lastTickPlayable=tickType!=2&&delta>0.f;lastTickAt=GetTickCount64();
    if(originalTick)originalTick(world,tickType,delta);

    if(outer&&consoleSupported.load())Service(world);
    if(outer&&quickSupported.load()&&OnGameThread())ServiceQuickSave(world);
    if(outer&&ironSupported.load())ServiceIronSights();
    if(outer)busy=false;
}
}
void Initialize(Logger log) {
    logger=log;image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(!image||!HashMatches()) {
        if(logger)logger(L"Native bridge unavailable: unsupported executable SHA-256. No native console calls enabled.");return;
    }
    constexpr uint8_t consoleDispatch[]={0xff,0x90,0xe0,0x0d,0x00,0x00};
    constexpr uint8_t getter[]={0x83,0xb9,0x20,0x02,0x00,0x00,0x00,0x7e,0x0c};
    constexpr uint8_t threadSetter[]={0x89,0x05,0x0f,0x71,0x65,0x05};
    constexpr uint8_t tick[]={0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x55,0x56,0x57,0x41,0x54};
    constexpr uint8_t moveGetter[]={0xff,0x90,0x30,0x08,0x00,0x00};
    constexpr uint8_t lookGetter[]={0xff,0x90,0x48,0x08,0x00,0x00};
    constexpr uint8_t viewTarget[]={0xff,0x90,0xa8,0x07,0x00,0x00};
    struct Check {uintptr_t rva;const uint8_t* bytes;size_t size;};
    const Check checks[]={{0x39E162E,consoleDispatch,sizeof(consoleDispatch)},
        {0x3FA9D70,getter,sizeof(getter)},{0x40861D3,threadSetter,sizeof(threadSetter)},
        {0x3A6CFF0,tick,sizeof(tick)},{0x37E0611,viewTarget,sizeof(viewTarget)},
        {0x37E0EB1,moveGetter,sizeof(moveGetter)},{0x37E0E81,lookGetter,sizeof(lookGetter)}};
    for(const auto& c:checks) {
        const void* p=reinterpret_cast<void*>(image+c.rva);
        if(!Range(p,c.size)||std::memcmp(p,c.bytes,c.size)) {
            if(logger)logger(L"Native bridge unavailable: verified native signatures differ.");return;
        }
    }
    supported=true;
    void* target=reinterpret_cast<void*>(image+0x3A6CFF0);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&HookWorldTick),reinterpret_cast<void**>(&originalTick))!=MH_OK||
       MH_EnableHook(target)!=MH_OK) {
        if(logger)logger(L"Native console unavailable: world-tick hook could not be installed.");return;
    }
    constexpr uint8_t manager[]={0x48,0x8b,0x81,0x60,0x0b,0x00,0x00};
    constexpr uint8_t weapon[]={0x48,0x8b,0x81,0xd8,0x00,0x00,0x00};
    constexpr uint8_t scope[]={0x0f,0xb6,0x81,0x3e,0x01,0x00,0x00};
    weaponSupported=std::memcmp(reinterpret_cast<void*>(image+0x4CC0395),manager,sizeof(manager))==0&&
        std::memcmp(reinterpret_cast<void*>(image+0x4CE0415),weapon,sizeof(weapon))==0&&
        std::memcmp(reinterpret_cast<void*>(image+0x4CE0695),scope,sizeof(scope))==0;
    constexpr uint8_t event[]={0x40,0x55,0x56,0x57,0x41,0x54,0x41,0x55,0x41,0x56,0x41,0x57};
    constexpr uint8_t viewport[]={0xf6,0x81,0xd9,0x00,0x00,0x00,0x20};
    constexpr uint8_t subsystemCall[]={0xe8,0xf1,0x0d,0xf6,0xff};
    constexpr uint8_t containsCall[]={0xe8,0xa1,0xb2,0xf6,0xff};
    if(std::memcmp(reinterpret_cast<void*>(image+0x147D520),event,sizeof(event))==0&&
       std::memcmp(reinterpret_cast<void*>(image+0x3279335),viewport,sizeof(viewport))==0&&
       std::memcmp(reinterpret_cast<void*>(image+0x327934A),subsystemCall,sizeof(subsystemCall))==0&&
       std::memcmp(reinterpret_cast<void*>(image+0x327935A),containsCall,sizeof(containsCall))==0&&
       MH_CreateHook(reinterpret_cast<void*>(image+0x147D520),reinterpret_cast<void*>(&HookEvent),reinterpret_cast<void**>(&originalEvent))==MH_OK&&
       MH_EnableHook(reinterpret_cast<void*>(image+0x147D520))==MH_OK)closeSupported=true;
    constexpr uint8_t saveGetter[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20};
    constexpr uint8_t autoSave[]={0x48,0x89,0x5c,0x24,0x08,0x57,0x48,0x83,0xec,0x20,0x48,0x8b,0x59,0x28};
    constexpr uint8_t slotZero[]={0x33,0xd2,0x48,0x8b,0xcf,0xe8,0x32,0x9e,0x02,0x00};
    constexpr uint8_t saveBody[]={0x40,0x55,0x41,0x56,0x41,0x57};
    constexpr uint8_t wrapperJump[]={0xe9,0xb6,0x9e,0x0b,0x00};
    quickSupported=std::memcmp(reinterpret_cast<void*>(image+0x4DD2FF0),saveGetter,sizeof(saveGetter))==0&&
        std::memcmp(reinterpret_cast<void*>(image+0x4D87040),autoSave,sizeof(autoSave))==0&&
        std::memcmp(reinterpret_cast<void*>(image+0x4D870C4),slotZero,sizeof(slotZero))==0&&
        std::memcmp(reinterpret_cast<void*>(image+0x4DB0F00),saveBody,sizeof(saveBody))==0&&
        std::memcmp(reinterpret_cast<void*>(image+0x4CCD185),wrapperJump,sizeof(wrapperJump))==0;
    constexpr uint8_t fullAim[]={0x40,0x53,0x55,0x57,0x48,0x83,0xec,0x50,0x33,0xff,0x0f,0xb6,0xea};
    // Native bool setter uses RCX=manager, DL=state; leaves ForceFullyAim/timers alone.
    ironSupported=weaponSupported.load()&&
        std::memcmp(reinterpret_cast<void*>(image+0x4D68EF0),fullAim,sizeof(fullAim))==0;
    consoleSupported=true;
    if(logger)logger(L"V0.9C native console queue ready. Explicit F1/menu requests only; game-thread gate; no input simulation or door/progression hook.");
}
bool Ready() {return supported.load();}
bool ConsoleReady() {return consoleSupported.load();}
bool QueueShop(HWND window) {
    if(!consoleSupported.load()||!window||GetForegroundWindow()!=window)return false;
    DWORD process=0;GetWindowThreadProcessId(window,&process);
    if(process!=GetCurrentProcessId())return false;
    uint64_t expected=0;
    requestedWindow=window;
    if(!pendingAt.compare_exchange_strong(expected,GetTickCount64()))return false;
    ++requests;return true;
}
bool IronSightsReady(){return ironSupported.load();}
bool IronSightsEnabled(){return ironEnabled.load();}
void SetIronSightsEnabled(bool enabled){ironEnabled=enabled;}
void MarkIronSightAim(void* owner,bool aiming){ironOwner=owner;ironAiming=aiming;ironSeenAt=GetTickCount64();}
bool QuickSaveReady(){return quickSupported.load();}
void MarkGameplayForSave(void* owner){playableOwner=owner;playableAt=GetTickCount64();}
bool QueueQuickSave(HWND window) {
    if(!quickSupported.load()||!window||GetForegroundWindow()!=window)return false;
    DWORD process=0;GetWindowThreadProcessId(window,&process);if(process!=GetCurrentProcessId())return false;
    uint64_t expected=0;quickWindow=window;
    if(!quickAt.compare_exchange_strong(expected,GetTickCount64()))return false;
    SetQuickStatus(QuickStatus::Queued,L"F4: explicit quick-save request queued.");return true;
}
const char* QuickSaveStatus(){
    switch(quickStatus.load()){
    case QuickStatus::Queued:return "Quick save queued...";
    case QuickStatus::Unavailable:return "Quick save unavailable here (gameplay required).";
    case QuickStatus::BackupFailed:return "Quick save cancelled: backup failed.";
    case QuickStatus::Requested:return "Saving through the game's autosave...";
    case QuickStatus::Written:return "Autosave file updated.";
    case QuickStatus::NoWrite:return "No autosave update detected.";
    case QuickStatus::Busy:return "Save already pending; please wait.";
    default:return "F4 saves to the game's AutoSave slot.";
    }
}
ULONGLONG QuickSaveStatusAt(){return quickStatusAt.load();}
bool ShopPending() {return pendingAt.load()!=0;}
unsigned Requests() {return requests.load();}
unsigned Dispatches() {return dispatches.load();}
unsigned Cancellations() {return cancellations.load();}
bool ShopCloseReady(){return closeSupported.load();}
bool PlayerWeaponState(void* owner,bool& sniper,bool& scopeShown) {
    sniper=false;scopeShown=false;
    if(!weaponSupported.load()||!OnGameThread()||!LiveObject(owner))return false;
    void* manager=nullptr;void* weapon=nullptr;uint8_t scope=0;
    // Exact read-only getter loads checked during initialization.
    if(!Read(owner,0xB60,manager)||!LiveObject(manager)||
       !Read(manager,0xD8,weapon)||!LiveObject(weapon)||!Read(manager,0x13E,scope)||scope>1)return false;
    sniper=DerivedFrom(weapon,"CRDivineSniper_BP_C");scopeShown=scope!=0;
    return true;
}
bool PlayerCameraControllable(void* owner) {
    if(!supported.load()||!OnGameThread()||!lastTickPlayable.load()||
       GetTickCount64()-lastTickAt.load()>500)return false;
    void* controller=nullptr;bool cinematic=true,cursor=true;
    if(!CameraController(owner,controller)||!ControllerCinematic(controller,cinematic)||cinematic||
       !ControllerCursor(controller,cursor)||cursor)return false;
    void* engine=nullptr;
    if(!Read(reinterpret_cast<void*>(image),0x9944860,engine)||!LiveObject(engine))return false;
    void* world=reinterpret_cast<WorldFromContextFn>(image+0x3F2BC10)(engine,owner,0);
    if(world!=lastTickWorld.load())return false;
    // execIsMoveInputIgnored 037E0EB1 and execIsLookInputIgnored 037E0E81:
    // one-argument virtual getters returning bool in AL. No FFrame calls.
    void* table=nullptr;void* target=nullptr;void* move=nullptr;void* look=nullptr;
    if(!Read(controller,0,table)||!Read(table,0x7A8,target)||
       !Read(table,0x830,move)||!Read(table,0x848,look))return false;
    for(void* fn:{target,move,look}) {
        const uintptr_t address=reinterpret_cast<uintptr_t>(fn);
        if(address<image+0x1000||address>=image+0x7490000||!Range(fn,16))return false;
    }
    using InputIgnoredFn=bool(__fastcall*)(void*);
    if(reinterpret_cast<InputIgnoredFn>(move)(controller)||
       reinterpret_cast<InputIgnoredFn>(look)(controller))return false;
    return reinterpret_cast<ViewTargetFn>(target)(controller)==owner;
}
bool ActiveCinematicCamera(void* owner,bool dedicatedCineComponent,bool& cinematic) {
    if(!supported.load()||!OnGameThread())return false;
    void* controller=nullptr;
    if(!CameraController(owner,controller)) {
        void* engine=nullptr;
        if(!LiveObject(owner)||!Read(reinterpret_cast<void*>(image),0x9944860,engine)||!LiveObject(engine))return false;
        // The same UEngine world-context getter used by ExecuteConsoleCommand.
        void* world=reinterpret_cast<WorldFromContextFn>(image+0x3F2BC10)(engine,owner,0);
        if(!LiveObject(world)||!DerivedFrom(world,"World"))return false;
        controller=reinterpret_cast<FirstControllerFn>(image+0x3FA9D70)(world);
        if(!LiveObject(controller)||!DerivedFrom(controller,"PlayerController"))return false;
    }
    bool mode=false;
    const bool known=ControllerCinematic(controller,mode);
    if(!dedicatedCineComponent&&(!known||!mode)) {cinematic=false;return known;}
    void* table=nullptr;void* target=nullptr;
    if(!Read(controller,0,table)||!Read(table,0x7A8,target))return false;
    const uintptr_t address=reinterpret_cast<uintptr_t>(target);
    if(address<image+0x1000||address>=image+0x7490000||!Range(target,16))return false;
    // execGetViewTarget (037E05F0) calls this one-argument virtual getter.
    if(reinterpret_cast<ViewTargetFn>(target)(controller)!=owner)return false;
    cinematic=true;return true;
}
}
