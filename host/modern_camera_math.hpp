#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
namespace portablekit::fu::modern_camera::math {
inline float curve(float axis, float exponent) {
    return std::copysign(std::pow(std::min(std::abs(axis),1.f),exponent),axis);
}
// Integrate the exponential exactly, not the endpoint velocity times dt.
// This makes both acceleration and release travel independent of frame rate.
inline float travel(float &velocity,float target,float seconds,float tau) {
    if(seconds<=0)return 0;
    if(tau<=0){velocity=target;return target*seconds;}
    const float blend=-std::expm1(-seconds/tau),previous=velocity;
    velocity+=(target-velocity)*blend;
    return target*seconds+(previous-target)*tau*blend;
}
inline float pitch_step(float pitch,float delta,float low,float high) {
    // Integrate the final five degrees exponentially; moving away is immediate.
    if(delta>0) {
        const float straight=std::min(delta,std::max(0.f,high-5.f-pitch));
        pitch+=straight;delta-=straight;
        if(delta>0)pitch=high-(high-pitch)*std::exp(-delta/5.f);
    } else if(delta<0) {
        const float straight=std::min(-delta,std::max(0.f,pitch-low-5.f));
        pitch-=straight;delta+=straight;
        if(delta<0)pitch=low+(pitch-low)*std::exp(delta/5.f);
    }
    return std::clamp(pitch,low,high);
}
struct Geometry {float height{},distance{};};
inline Geometry geometry(float pitch,float distance,float focus) {
    const float radians=pitch*(3.14159265358979323846f/180.f);
    return {focus+distance*std::sin(radians),distance*std::cos(radians)};
}
enum class Framing { Normal, Low, Excluded };
inline Framing framing(unsigned state,float native_height,float native_focus) {
    if(state!=0 && state!=3)return Framing::Excluded;
    // At the geometry hook vanilla has already evaluated its action predicates.
    return state==3 || (native_height==20.f && std::abs(native_focus-30.f)<.01f)
        ?Framing::Low:Framing::Normal;
}
inline bool within_grace(std::uint64_t now,std::uint64_t last) {
    return last && now>=last && now-last<=250000000;
}
}
