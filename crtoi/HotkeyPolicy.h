#pragma once
#include <windows.h>
namespace crtoi::hotkeys {
inline bool PassSystemClose(UINT message,WPARAM key,LPARAM bits,bool altDown){
    const bool keyMessage=message==WM_SYSKEYDOWN||message==WM_SYSKEYUP||message==WM_KEYDOWN||message==WM_KEYUP;
    return keyMessage&&key==VK_F4&&(altDown||(bits&(LPARAM(1)<<29))!=0);
}
enum class QuickAction {None,Save,Load};
inline QuickAction GetQuickAction(UINT message,WPARAM key,LPARAM bits,bool alt,bool control,bool shift,int saveKey=VK_F5,int loadKey=VK_F8){
    if(message!=WM_KEYDOWN||alt||control||shift||(bits&(LPARAM(1)<<29))||(bits&(LPARAM(1)<<30)))return QuickAction::None;
    if(key==WPARAM(saveKey))return QuickAction::Save;
    if(key==WPARAM(loadKey))return QuickAction::Load;
    return QuickAction::None;
}
}
