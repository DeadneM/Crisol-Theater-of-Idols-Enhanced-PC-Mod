#include "../NativeBridge.h"
#include <cstdio>

namespace {bool logged=false;void Log(const wchar_t*){logged=true;}}
int main() {
    using namespace crtoi::nativebridge;
    // This test executable is deliberately NOT the supported game. Even poisoned
    // contexts must be refused before any game object access or native call.
    Initialize(&Log);
    if(Ready()||ConsoleReady()||!logged)return 1;
    bool cinematic=false;
    if(ActiveCinematicCamera(reinterpret_cast<void*>(1),false,cinematic))return 2;
    if(ActiveCinematicCamera(reinterpret_cast<void*>(1),true,cinematic))return 2;
    if(QueueShop(reinterpret_cast<HWND>(1)))return 3;
    if(ShopPending()||Dispatches()||Requests())return 4;
    std::puts("Unsupported executable: poisoned context, command queue and native calls refused.");
    return 0;
}
