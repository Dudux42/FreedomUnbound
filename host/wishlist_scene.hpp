#pragma once
namespace psprecomp {class GuestMemory;}
namespace portablekit::gpu {struct DrawCall;}
namespace portablekit::fu::wishlist {
inline bool blocks_scene(bool textured,unsigned low_alpha,unsigned high_alpha,bool black) {
    // The village also draws a permanent translucent white lighting gradient.
    // Only opaque images/solids and uniform black fades conceal the scene.
    return textured ? high_alpha>=240 : high_alpha>0 && low_alpha==high_alpha && (black || high_alpha>=240);
}
// A loaded overlay is not yet a visible village. Require an actual world draw
// for the current context, with no loading image or fade drawn over it.
class SceneGate {
    double last_{-1};bool town_{},covered_{true};
public:
    void world(bool town,double now){town_=town;last_=now;covered_=false;}
    void cover(){covered_=true;}
    bool visible(bool town,double now) const{return !covered_ && last_>=0 && town==town_ && now>=last_ && now-last_<.25;}
};
void observe_scene(const gpu::DrawCall &,const psprecomp::GuestMemory &);
}
