#include "../HubReentry.h"
#include <array>
#include <cstdio>

int main() {
    // Table expresses the release boundary: no chapter-completion flag may
    // unlock a future entrance, interior gate, prologue or final chapter.
    struct Case {int chapter;bool end1,end2,expected;};
    constexpr std::array<Case,15> cases={{
        {1,false,false,false},{1,true,false,true},{1,true,true,true},
        {2,false,false,false},{2,true,false,false},{2,true,true,true},
        {3,false,false,false},{3,true,false,false},{3,true,true,false},
        {0,true,true,false},{-1,true,true,false},{4,true,true,false},
        {99,true,true,false},{1,false,true,true},{2,false,true,true}
    }};
    for(const auto& c:cases) {
        if(crtoi::hub::CanRevisit(c.chapter,c.end1,c.end2)!=c.expected) {
            std::fprintf(stderr,"Rejected policy boundary: chapter %d, end1=%d, end2=%d\n",c.chapter,int(c.end1),int(c.end2));
            return 1;
        }
    }
    using crtoi::hub::Scope;
    struct Route {Scope scope;int chapter;unsigned map;bool load,keep,end1,end2,expected;};
    constexpr std::array<Route,14> routes={{
        {Scope::Fair,1,2,true,true,true,false,true},
        {Scope::Fair,2,3,true,true,true,true,true},
        {Scope::Fair,3,4,true,true,true,true,false},
        {Scope::Fair,1,3,true,true,true,true,false},
        {Scope::Fair,1,2,false,true,true,true,false},
        {Scope::Fair,1,2,true,false,true,true,false},
        {Scope::Other,1,2,true,true,true,true,false},
        {Scope::Chapter1,0,1,true,true,true,false,true},
        {Scope::Chapter1,0,1,true,true,false,false,false},
        {Scope::Chapter2,0,1,true,true,true,false,false},
        {Scope::Chapter2,0,1,true,true,true,true,true},
        {Scope::Chapter2,0,2,true,true,true,true,false},
        {Scope::Chapter2,1,1,true,true,true,true,false},
        {Scope::Chapter2,0,1,true,false,true,true,false}
    }};
    for(const auto& r:routes) {
        if(crtoi::hub::CanUseExistingRoute(r.scope,r.chapter,r.map,r.load,r.keep,r.end1,r.end2)!=r.expected) {
            std::fputs("Wrong map, incomplete chapter or player-data boundary failed.\n",stderr);return 1;
        }
    }
    std::puts("29 completed-chapter and return-route policy boundaries passed.");
    return 0;
}
