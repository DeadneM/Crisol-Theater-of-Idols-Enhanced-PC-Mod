#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <MinHook.h>
#include "HubReentry.h"
#include <array>
#include <atomic>
#include <cstring>
#include <iterator>
#include <limits>

namespace crtoi::hub {
namespace {
std::atomic_bool enabled{false},ready{false};
std::atomic_uint seen{0},returnsSeen{0},permissions{0},locks{0},rejected{0};
std::atomic_uint permissionName{UINT32_MAX},lockName{UINT32_MAX};
Logger logger=nullptr;
uintptr_t image=0;
// All addresses/layouts below are tied to the full executable hash, then
// independently checked against native code before installing this hook.
constexpr uintptr_t kProcessInternal=0x147DA20;
constexpr uintptr_t kGetCRGameInstance=0x4DCFC50;
constexpr uintptr_t kNamePool=0x96E8600;
using ScriptFn=void(__fastcall*)(void*,void*,void*);
using GetInstanceFn=void*(__fastcall*)(const void*);
ScriptFn original=nullptr;

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
bool NamedPrefix(const void* ptr,const char* prefix) {
    uint32_t id=0,number=0;
    return Read(ptr,0x18,id)&&Read(ptr,0x1c,number)&&!number&&NameEquals(id,prefix,true);
}
Scope DoorScope(void* object,void* function,void*& cls) {
    void* owner=nullptr;
    if(!Read(object,0x10,cls)||!Named(cls,0x18,"CR_Teleport_BP_C")||
       !Read(function,0x20,owner)||owner!=cls)return Scope::Other;
    void* outer=object;
    for(int depth=0;depth<8&&outer;++depth) {
        if(Named(outer,0x18,"/Game/GameContent/Maps/GameMap/Lobby/Blocking/00_Lobby_Blocking_SLI"))return Scope::Fair;
        if(Named(outer,0x18,"/Game/GameContent/Maps/GameMap/Chapter_1/Blocking/01_Chapter01_BlockingWP_LIB1")||
           NamedPrefix(outer,"/Game/GameContent/Maps/GameMap/Chapter_1/Blocking/01_Chapter01_BlockingWP_LIB1/_Generated_/"))return Scope::Chapter1;
        if(Named(outer,0x18,"/Game/GameContent/Maps/GameMap/Chapter_2/Blocking/02_Chapter02_Blocking_LIB")||
           NamedPrefix(outer,"/Game/GameContent/Maps/GameMap/Chapter_2/Blocking/02_Chapter02_Blocking_LIB/_Generated_/"))return Scope::Chapter2;
        if(!Read(outer,0x20,outer))return Scope::Other;
    }
    return Scope::Other;
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
bool BoolValue(void* object,void* structure,const char* name,bool& value) {
    Property p;uint8_t raw=0;
    if(!FindProperty(structure,name,"BoolProperty",1,p,true)||
       !Read(object,size_t(p.offset)+p.byteOffset,raw))return false;
    value=(raw&p.fieldMask)!=0;return true;
}
bool Eligible(void* object,void* cls,Scope scope,int& chapter) {
    Property c,m;uint8_t map=0;bool load=false,keep=false;
    if(!FindProperty(cls,"ChapterNum","IntProperty",4,c,true)||
       !Read(object,size_t(c.offset),chapter)||chapter<0||chapter>3)return false;
    if(scope==Scope::Fair&&chapter>=1)seen.fetch_or(1u<<unsigned(chapter-1));
    if(!FindProperty(cls,"MapToLoad","ByteProperty",1,m,true)||
       !Read(object,size_t(m.offset),map)||
       !BoolValue(object,cls,"LoadLevel",load)||
       !BoolValue(object,cls,"Keep Player Data",keep))return false;
    // Verified native pure getter: generated execGetCRGameInstance marshals
    // exactly one UObject* context, calls this function and returns UObject*.
    auto get=reinterpret_cast<GetInstanceFn>(image+kGetCRGameInstance);
    void* instance=get(object);void* instanceClass=nullptr;
    bool end1=false,end2=false;
    if(!Read(instance,0x10,instanceClass)||
       !BoolValue(instance,instanceClass,"EndChapter1",end1)||
       !BoolValue(instance,instanceClass,"EndChapter2",end2))return false;
    if(scope==Scope::Chapter1&&chapter==0&&map==1)returnsSeen.fetch_or(1);
    if(scope==Scope::Chapter2&&chapter==0&&map==1)returnsSeen.fetch_or(2);
    return CanUseExistingRoute(scope,chapter,map,load,keep,end1,end2);
}
uint8_t* BoolAddress(void* container,const Property& p) {
    if(!container)return nullptr;
    auto* ptr=static_cast<uint8_t*>(container)+p.offset+p.byteOffset;
    return Range(ptr,1,true)?ptr:nullptr;
}
void StoreBool(uint8_t* ptr,const Property& p,bool value) {
    *ptr=uint8_t((*ptr&~p.fieldMask)|(value?p.byteMask:0));
}
void __fastcall Hook(void* object,void* frame,void* result) {
    if(!original)return;
    if(!enabled.load(std::memory_order_relaxed)) {original(object,frame,result);return;}
    void* function=nullptr;void* frameObject=nullptr;uint32_t id=0,number=0;
    if(!Read(frame,0x10,function)||!Read(function,0x18,id)||
       !Read(function,0x1c,number)||number||!Read(frame,0x18,frameObject)||frameObject!=object) {original(object,frame,result);return;}
    bool permission=id==permissionName.load(std::memory_order_relaxed);
    bool lock=id==lockName.load(std::memory_order_relaxed);
    if(!permission&&!lock) {
        permission=NameEquals(id,"IsCorrectDoor");lock=NameEquals(id,"SetLockState");
        if(permission)permissionName=id;
        if(lock)lockName=id;
    }
    if(!permission&&!lock) {original(object,frame,result);return;}
    void* cls=nullptr;int chapter=0;int32_t scriptSize=0;uint16_t paramSize=0;
    const Scope scope=DoorScope(object,function,cls);
    if(scope==Scope::Other||
       !Read(function,0x68,scriptSize)||scriptSize!=(permission?286:480)||
       !Read(function,0xb6,paramSize)||paramSize!=1||!Eligible(object,cls,scope,chapter)) {
        original(object,frame,result);return;
    }
    Property parameter;
    if(!FindProperty(function,permission?"Out":"Locked","BoolProperty",1,parameter,false)||parameter.offset!=0) {
        ++rejected;original(object,frame,result);return;
    }
    if(lock) {
        void* locals=nullptr;uint8_t* value=nullptr;
        if(Read(frame,0x28,locals))value=BoolAddress(locals,parameter);
        if(!value) {++rejected;original(object,frame,result);return;}
        const uint8_t before=*value;
        const bool changed=(before&parameter.fieldMask)!=0;
        if(changed)StoreBool(value,parameter,false);
        original(object,frame,result);
        // Restore the caller's parameter bytes. Only the game's normal door
        // script saw an unlocked argument; no progression flags were written.
        if(changed) {
            *value=before;
            if(locks.fetch_add(1)==0&&logger)logger(L"Hub reentry: native SetLockState saw an unlocked argument for a completed chapter route. Caller parameter restored.");
        }
        return;
    }
    original(object,frame,result);
    void* out=nullptr;
    if(!Read(frame,0x80,out)) {++rejected;return;}
    for(unsigned n=0;out&&n<8;++n) {
        void* property=nullptr;void* address=nullptr;
        if(!Read(out,0,property)||!Read(out,8,address))break;
        if(property==parameter.address) {
            // FOutParmRec.PropAddr already includes Offset_Internal.
            Property relative=parameter;relative.offset=0;
            auto* value=BoolAddress(address,relative);
            if(value&&(*value&relative.fieldMask)==0) {
                StoreBool(value,relative,true);
                if(permissions.fetch_add(1)==0&&logger)logger(L"Hub reentry: IsCorrectDoor allowed a completed chapter route. Progression flags unchanged.");
            }
            return;
        }
        if(!Read(out,0x10,out))break;
    }
    ++rejected;
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
}
void Install(Logger log) {
    logger=log;image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(!image||!HashMatches()) {if(logger)logger(L"Hub reentry unavailable: unsupported executable SHA-256.");return;}
    constexpr uint8_t scriptBytes[]={0x48,0x89,0x5c,0x24,0x08,0x55,0x56,0x57,0x48,0x83,0xec,0x70};
    constexpr uint8_t getterBytes[]={0x40,0x53,0x48,0x83,0xec,0x20,0xe8,0x05,0x63,0xad,0xfe};
    const auto* script=reinterpret_cast<const void*>(image+kProcessInternal);
    const auto* getter=reinterpret_cast<const void*>(image+kGetCRGameInstance);
    if(!Range(script,sizeof(scriptBytes))||!Range(getter,sizeof(getterBytes))||
       std::memcmp(script,scriptBytes,sizeof(scriptBytes))||std::memcmp(getter,getterBytes,sizeof(getterBytes))) {
        if(logger)logger(L"Hub reentry unavailable: native signatures differ.");return;
    }
    void* target=reinterpret_cast<void*>(image+kProcessInternal);
    if(MH_CreateHook(target,reinterpret_cast<void*>(&Hook),reinterpret_cast<void**>(&original))!=MH_OK||
       MH_EnableHook(target)!=MH_OK) {if(logger)logger(L"Hub reentry unavailable: hook installation failed.");return;}
    ready=true;
    if(logger)logger(L"Hub reentry TEST hook ready. Disabled by default; enable then reload the hub save. Only completed chapter 1/2 entrances and their existing return doors are eligible; chapter 3 stays native.");
}
void SetEnabled(bool value) {enabled=value;}
bool Enabled() {return enabled.load();}
bool Ready() {return ready.load();}
unsigned SeenChapters() {return seen.load();}
unsigned SeenReturns() {return returnsSeen.load();}
unsigned PermissionChanges() {return permissions.load();}
unsigned LockChanges() {return locks.load();}
unsigned RejectedSchemas() {return rejected.load();}
}
