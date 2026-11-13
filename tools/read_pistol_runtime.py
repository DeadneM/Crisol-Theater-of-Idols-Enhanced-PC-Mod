"""Read-only Crisol animation snapshot. Never writes to the game process or saves."""
import argparse, ctypes as c, hashlib, json, struct, time
from ctypes import wintypes as w
from pathlib import Path

EXE = "CRToiPrototype-Win64-Shipping.exe"
HASH = "b8422259fde6aaae65f2075c9d26a60abfb1ac4c0f231217d458bfe28418d378"
k = c.WinDLL("kernel32", use_last_error=True)
class Process(c.Structure):
    _fields_=[("size",w.DWORD),("usage",w.DWORD),("pid",w.DWORD),("heap",c.c_size_t),("module",w.DWORD),("threads",w.DWORD),("parent",w.DWORD),("priority",w.LONG),("flags",w.DWORD),("exe",w.WCHAR*260)]
class Module(c.Structure):
    _fields_=[("size",w.DWORD),("id",w.DWORD),("pid",w.DWORD),("global_use",w.DWORD),("process_use",w.DWORD),("base",c.c_void_p),("length",w.DWORD),("module",w.HMODULE),("name",w.WCHAR*256),("path",w.WCHAR*260)]
k.CreateToolhelp32Snapshot.argtypes=[w.DWORD,w.DWORD]; k.CreateToolhelp32Snapshot.restype=w.HANDLE
k.Process32FirstW.argtypes=[w.HANDLE,c.POINTER(Process)]; k.Process32NextW.argtypes=k.Process32FirstW.argtypes
k.Module32FirstW.argtypes=[w.HANDLE,c.POINTER(Module)]; k.Module32NextW.argtypes=k.Module32FirstW.argtypes
k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD]; k.OpenProcess.restype=w.HANDLE
k.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
k.CloseHandle.argtypes=[w.HANDLE]

def entries(flags,pid,kind,first,next_):
    h=k.CreateToolhelp32Snapshot(flags,pid)
    if h==c.c_void_p(-1).value: raise OSError(c.get_last_error(),"Process inventory unavailable")
    try:
        item=kind(); item.size=c.sizeof(kind); ok=first(h,c.byref(item))
        while ok:
            yield item
            item=kind(); item.size=c.sizeof(kind); ok=next_(h,c.byref(item))
    finally:k.CloseHandle(h)

class Reader:
    def __init__(self,pid,base):
        # PROCESS_VM_READ | PROCESS_QUERY_LIMITED_INFORMATION. No write/execute rights.
        self.h=k.OpenProcess(0x1010,False,pid); self.base=base; self.names={}; self.fields_cache={}
        if not self.h:raise OSError(c.get_last_error(),"Read-only game access denied")
    def close(self):k.CloseHandle(self.h)
    def read(self,at,n):
        if not at or n<0 or n>1048576:raise ValueError("Invalid bounded read")
        buf=c.create_string_buffer(n); got=c.c_size_t()
        if not k.ReadProcessMemory(self.h,c.c_void_p(at),buf,n,c.byref(got)) or got.value!=n:raise OSError("Unreadable game object")
        return buf.raw
    def u64(self,p):return struct.unpack('<Q',self.read(p,8))[0]
    def u32(self,p):return struct.unpack('<I',self.read(p,4))[0]
    def name(self,index):
        if index in self.names:return self.names[index]
        if index>>16>=8192:raise ValueError("Invalid FName block")
        block=self.u64(self.base+0x96E8600+0x10+(index>>16)*8)
        at=block+(index&65535)*2; header=struct.unpack('<H',self.read(at,2))[0]; length=header>>6
        if not 0<length<=1023:raise ValueError("Invalid FName length")
        text=self.read(at+2,length*(2 if header&1 else 1)).decode('utf-16-le' if header&1 else 'utf-8',errors='replace')
        self.names[index]=text;return text
    def label(self,obj):
        if not obj:return None
        try:return {"name":self.name(self.u32(obj+0x18)),"class":self.name(self.u32(self.u64(obj+0x10)+0x18))}
        except (OSError,ValueError):return None
    def fields(self,cls):
        if cls in self.fields_cache:return self.fields_cache[cls]
        result={}; current=cls
        for depth in range(8):
            if not current:break
            total=self.u32(current+0x58)
            if not 0<total<=0x200000:break
            field=self.u64(current+0x50)
            for _ in range(512):
                if not field:break
                dim=self.u32(field+0x30); size=self.u32(field+0x34); offset=self.u32(field+0x44)
                if dim==1 and 0<size<=4096 and offset+size<=total:
                    name=self.name(self.u32(field+0x20)); typ=self.name(self.u32(self.u64(field+8)))
                    result.setdefault(name,{"field":field,"type":typ,"offset":offset,"size":size})
                field=self.u64(field+0x18)
            current=self.u64(current+0x40)
        self.fields_cache[cls]=result;return result
    def prop(self,obj,name):return self.fields(self.u64(obj+0x10)).get(name)
    def object(self,obj,name):
        p=self.prop(obj,name)
        if not p or p['type']!='ObjectProperty' or p['size']!=8:raise ValueError("Missing object field: "+name)
        value=self.u64(obj+p['offset'])
        if not self.label(value):raise ValueError("Invalid object field: "+name)
        return value
    def anims(self,obj,name):
        p=self.prop(obj,name)
        if not p or p['type']!='StructProperty':return {"status":"field unavailable"}
        # Validate the actual referenced script struct before naming any members.
        schema=self.u64(p['field']+0x70); label=self.label(schema)
        if not label or label['class']!='ScriptStruct' or self.u32(schema+0x58)!=p['size']:
            return {"status":"struct schema unconfirmed","size":p['size']}
        result={}
        for field,desc in self.fields(schema).items():
            if desc['type']=='ObjectProperty' and desc['size']==8:
                result[field]=self.label(self.u64(obj+p['offset']+desc['offset']))
        return {"schema":label,"members":result}
    def pose(self,obj):
        result={}
        for name in ('RelativeLocation','RelativeRotation'):
            p=self.prop(obj,name)
            if p and p['type']=='StructProperty' and p['size']==24:result[name]=struct.unpack('<3d',self.read(obj+p['offset'],24))
        return result
    def states(self,obj):
        result={}
        for name in ('bIsFullyAim','bAimTransition'):
            p=self.prop(obj,name)
            if p and p['type']=='BoolProperty' and p['size']==1:
                off,mask,fieldmask=self.read(p['field']+0x71,3)
                if off<1 and mask and fieldmask:result[name]=bool(self.read(obj+p['offset']+off,1)[0]&fieldmask)
        return result

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--output',type=Path,required=True);ap.add_argument('--seconds',type=float,default=8);ap.add_argument('--delay',type=float,default=5);args=ap.parse_args()
    if not 0<=args.delay<=15 or not 1<=args.seconds<=15:raise ValueError("Capture duration out of bounds")
    if args.output.suffix.lower()!=".json" or args.output.exists():raise ValueError("Choose a new JSON report path; overwriting is prohibited")
    matches=[p.pid for p in entries(2,0,Process,k.Process32FirstW,k.Process32NextW) if p.exe.lower()==EXE.lower()]
    if len(matches)!=1:raise RuntimeError("Open exactly one Crisol game instance with the pistol equipped")
    module=next((m for m in entries(0x18,matches[0],Module,k.Module32FirstW,k.Module32NextW) if m.name.lower()==EXE.lower()),None)
    if module is None:raise RuntimeError("Game module not found")
    if args.output.resolve().is_relative_to(Path(module.path).resolve().parents[3]):raise ValueError('Report must be outside the game directory')
    with Path(module.path).open('rb') as image:digest=hashlib.file_digest(image,'sha256').hexdigest()
    if digest!=HASH:raise RuntimeError("Unsupported game executable; no metadata read")
    r=Reader(matches[0],module.base)
    try:
        for offset,expected in [(0x4CC0395,bytes.fromhex('488b81600b0000')),(0x4CE0415,bytes.fromhex('488b81d8000000'))]:
            if r.read(module.base+offset,len(expected))!=expected:raise RuntimeError("Loaded image signature mismatch")
        time.sleep(args.delay)
        engine=r.u64(module.base+0x9944860); viewport=r.object(engine,'GameViewport'); instance=r.object(viewport,'GameInstance')
        p=r.prop(instance,'LocalPlayers')
        if not p or p['type']!='ArrayProperty' or p['size']!=16:raise RuntimeError("Local player schema unavailable")
        data=r.u64(instance+p['offset']);count=r.u32(instance+p['offset']+8)
        if not 1<=count<=4:raise RuntimeError("No local player")
        controller=r.object(r.u64(data),'PlayerController'); pawn=r.object(controller,'Pawn')
        manager=r.u64(pawn+0xB60);weapon=r.u64(manager+0xD8);arms=r.u64(manager+0x100);mesh=r.u64(weapon+0x2B0)
        label=r.label(weapon)
        if not label or 'Pistol' not in label['class']:raise RuntimeError("Equip the pistol before capture: "+str(label))
        report={"mode":"read-only","weapon":label,"arms":r.label(arms),"weapon_anims":r.anims(weapon,'WeaponAnims'),"arms_anims":r.anims(arms,'PlayerWeaponAnims'),"raw_return_slots":{str(i):r.label(r.u64(weapon+0x308+i*8)) for i in range(10)},"frames":[]}
        start=time.monotonic()
        while time.monotonic()-start<args.seconds:
            if r.u64(manager+0xD8)!=weapon:raise RuntimeError("Weapon changed during capture")
            report['frames'].append({"t":round(time.monotonic()-start,3),"states":r.states(arms),"weapon_relative":r.pose(mesh)})
            time.sleep(.1)
        args.output.parent.mkdir(parents=True,exist_ok=True)
        with args.output.open('x',encoding='utf-8') as output:json.dump(report,output,indent=2)
        print(json.dumps({"weapon":label,"frames":len(report['frames']),"output":str(args.output)}))
    finally:r.close()
if __name__=='__main__':main()
