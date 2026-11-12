#pragma once
#include <algorithm>
#include <cmath>
namespace crtoi::fov {
inline float EaseOffset(float current,float target,float seconds) {
    return current+(target-current)*(1.f-std::exp(-std::max(seconds,0.f)/0.15f));
}
inline float ApplyOffset(float nativeFov,float offset) {
    return std::clamp(nativeFov+offset,1.f,179.f);
}
}
