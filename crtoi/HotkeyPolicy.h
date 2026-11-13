#pragma once
#include <windows.h>
namespace crtoi::hotkeys {
inline bool PassSystemClose(UINT message,WPARAM key,LPARAM bits,bool altDown){
    const bool keyMessage=message==WM_SYSKEYDOWN||message==WM_SYSKEYUP||message==WM_KEYDOWN||message==WM_KEYUP;
    return keyMessage&&key==VK_F4&&(altDown||(bits&(LPARAM(1)<<29))!=0);
}
}
