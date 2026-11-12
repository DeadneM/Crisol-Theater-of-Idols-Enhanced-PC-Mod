#include "../FovPolicy.h"
#include <cmath>
#include <cstdio>
bool Near(float a,float b){return std::abs(a-b)<0.001f;}
int main(){
    using namespace crtoi::fov;
    // Synthetic values, not claims about this game's native aim values.
    if(!Near(ApplyOffset(90.f,30.f),120.f))return 1;
    if(!Near(ApplyOffset(65.f,30.f)-ApplyOffset(90.f,30.f),-25.f))return 2;
    if(!Near(ApplyOffset(100.f,30.f)-ApplyOffset(90.f,30.f),10.f))return 3;
    if(!Near(ApplyOffset(65.f,0.f),65.f))return 4;
    float coarse=0.f,fine=0.f;
    for(int i=0;i<30;++i)coarse=EaseOffset(coarse,30.f,1.f/30.f);
    for(int i=0;i<144;++i)fine=EaseOffset(fine,30.f,1.f/144.f);
    if(!Near(coarse,fine)||coarse<=0.f||coarse>=30.f)return 5;
    if(!Near(EaseOffset(12.f,30.f,0.f),12.f))return 6;
    if(ApplyOffset(170.f,30.f)!=179.f||ApplyOffset(10.f,-30.f)!=1.f)return 7;
    std::puts("Native aim delta, vanilla identity, projection bounds and frame-rate independent easing passed.");
}
