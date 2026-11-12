#pragma once
#include <cstdint>

namespace crtoi::hub {
using Logger=void(*)(const wchar_t*);
void Install(Logger log);
void SetEnabled(bool value);
bool Enabled();
bool Ready();
unsigned SeenChapters();
unsigned SeenReturns();
unsigned PermissionChanges();
unsigned LockChanges();
unsigned RejectedSchemas();

// Only previously completed chapter entrances can gain access. Chapter 3
// already remains the native active entrance after EndChapter2.
constexpr bool CanRevisit(int chapter,bool endChapter1,bool endChapter2) {
    return (chapter==1&&(endChapter1||endChapter2)) ||
           (chapter==2&&endChapter2);
}
enum class Scope {Other,Fair,Chapter1,Chapter2};
constexpr bool CanUseExistingRoute(Scope scope,int chapter,unsigned map,
                                  bool loadLevel,bool keepPlayerData,bool end1,bool end2) {
    if(!loadLevel||!keepPlayerData)return false;
    if(scope==Scope::Fair)return chapter>=1&&chapter<=3&&map==unsigned(chapter+1)&&CanRevisit(chapter,end1,end2);
    if(chapter!=0||map!=1)return false;
    if(scope==Scope::Chapter1)return CanRevisit(1,end1,end2);
    if(scope==Scope::Chapter2)return CanRevisit(2,end1,end2);
    return false;
}
}
