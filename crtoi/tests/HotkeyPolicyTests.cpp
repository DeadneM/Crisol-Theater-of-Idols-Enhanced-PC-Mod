#include "../HotkeyPolicy.h"
int main(){
    using crtoi::hotkeys::PassSystemClose;
    if(!PassSystemClose(WM_SYSKEYDOWN,VK_F4,LPARAM(1)<<29,false))return 1;
    if(!PassSystemClose(WM_KEYDOWN,VK_F4,0,true))return 2;
    if(!PassSystemClose(WM_SYSKEYUP,VK_F4,LPARAM(1)<<29,false))return 3;
    if(PassSystemClose(WM_KEYDOWN,VK_F4,0,false))return 4;
    if(PassSystemClose(WM_KEYDOWN,VK_F3,LPARAM(1)<<29,true))return 5;
    using namespace crtoi::hotkeys;
    if(GetQuickAction(WM_KEYDOWN,VK_F5,0,false,false,false)!=QuickAction::Save)return 6;
    if(GetQuickAction(WM_KEYDOWN,VK_F8,0,false,false,false)!=QuickAction::Load)return 7;
    if(GetQuickAction(WM_KEYDOWN,VK_F4,0,false,false,false)!=QuickAction::None)return 8;
    if(GetQuickAction(WM_KEYDOWN,VK_F5,0,true,false,false)!=QuickAction::None)return 9;
    if(GetQuickAction(WM_KEYDOWN,VK_F8,0,false,true,false)!=QuickAction::None)return 10;
    if(GetQuickAction(WM_KEYDOWN,VK_F5,LPARAM(1)<<30,false,false,false)!=QuickAction::None)return 11;
    if(GetQuickAction(WM_KEYDOWN,VK_F6,0,false,false,false,VK_F6,VK_F9)!=QuickAction::Save)return 12;
    if(GetQuickAction(WM_KEYDOWN,VK_F5,0,false,false,false,VK_F6,VK_F9)!=QuickAction::None)return 13;
}
