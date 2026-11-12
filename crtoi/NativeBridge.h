#pragma once
#include <windows.h>

namespace crtoi::nativebridge {
using Logger=void(*)(const wchar_t*);
void Initialize(Logger log);
bool Ready();
bool ConsoleReady();
bool QueueShop(HWND gameWindow);
bool ShopPending();
unsigned Requests();
unsigned Dispatches();
unsigned Cancellations();
bool PlayerWeaponState(void* cameraOwner,bool& sniper,bool& scopeShown);
bool ShopCloseReady();
bool PlayerCameraControllable(void* cameraOwner);
bool ActiveCinematicCamera(void* cameraOwner,bool dedicatedCineComponent,bool& cinematic);
}
