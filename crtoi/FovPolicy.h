#pragma once
#include <algorithm>
#include <cmath>
namespace crtoi::fov {
inline float EaseOffset(float current,float target,float seconds) {
    return current+(target-current)*(1.f-std::exp(-std::max(seconds,0.f)/0.15f));
}
inline float SelectTarget(bool aiming,bool sniper,float normal,float aim,float scope) {
    return aiming?(sniper?scope:aim):normal;
}
}
