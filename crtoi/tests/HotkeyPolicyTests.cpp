#include "../HotkeyPolicy.h"
int main(){
    using crtoi::hotkeys::PassSystemClose;
    if(!PassSystemClose(WM_SYSKEYDOWN,VK_F4,LPARAM(1)<<29,false))return 1;
    if(!PassSystemClose(WM_KEYDOWN,VK_F4,0,true))return 2;
    if(!PassSystemClose(WM_SYSKEYUP,VK_F4,LPARAM(1)<<29,false))return 3;
    if(PassSystemClose(WM_KEYDOWN,VK_F4,0,false))return 4;
    if(PassSystemClose(WM_KEYDOWN,VK_F3,LPARAM(1)<<29,true))return 5;
}
