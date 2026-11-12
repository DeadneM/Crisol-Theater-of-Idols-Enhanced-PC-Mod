#include "../FovPolicy.h"
#include <cmath>
#include <cstdio>
bool Near(float a,float b){return std::abs(a-b)<0.001f;}
int main(){
    using namespace crtoi::fov;
    if(SelectTarget(false,false,120,80,65)!=120)return 1;
    if(SelectTarget(true,false,120,80,65)!=80)return 2;
    if(SelectTarget(true,true,120,80,65)!=65)return 3;
    if(SelectTarget(false,true,120,80,65)!=120)return 4;
    float coarse=120.f,fine=120.f;
    for(int i=0;i<30;++i)coarse=EaseOffset(coarse,80.f,1.f/30.f);
    for(int i=0;i<144;++i)fine=EaseOffset(fine,80.f,1.f/144.f);
    if(!Near(coarse,fine)||coarse<=80.f||coarse>=120.f)return 5;
    float recovering=coarse;
    for(int i=0;i<60;++i){float next=EaseOffset(recovering,120.f,1.f/60.f);
        if(next<recovering||next>120.f)return 6;recovering=next;}
    if(!Near(EaseOffset(92.f,80.f,0.f),92.f))return 7;
    if(EaseOffset(120.f,80.f,1.f/60.f)==80.f)return 8;
    std::puts("Independent aim/sniper targets, smooth entry/exit and frame-rate independence passed.");
}
