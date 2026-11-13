#pragma once
#include <windows.h>
#include <string>

namespace crtoi::nativebridge {
using Logger=void(*)(const wchar_t*);
void Initialize(Logger log);
bool Ready();
bool ConsoleReady();
bool QueueShop(HWND gameWindow);
bool QueueConsoleCommand(HWND gameWindow,const char* text);
unsigned ConsoleReplyVersion();
std::string ConsoleReply();
bool QuickSaveReady();
bool QueueQuickSave(HWND gameWindow,bool fromOverlay=false);
bool QueueQuickLoad(HWND gameWindow,bool fromOverlay=false);
bool QuickLoadReady();
void MarkGameplayForSave(void* cameraOwner);
const char* QuickSaveStatus();
ULONGLONG QuickSaveStatusAt();
bool ShopPending();
unsigned Requests();
unsigned Dispatches();
unsigned Cancellations();
bool PlayerWeaponState(void* cameraOwner,bool& sniper,bool& scopeShown);
bool ShopCloseReady();
bool PlayerCameraControllable(void* cameraOwner);
bool ActiveCinematicCamera(void* cameraOwner,bool dedicatedCineComponent,bool& cinematic);
}
