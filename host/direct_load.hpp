#pragma once
#include "save_preview.hpp"
namespace psprecomp { class Runtime; }
namespace portablekit::gpu { struct PadState; }
namespace portablekit::fu::direct_load {
bool enabled();
void request(unsigned slot, const HunterSummary &hunter);
bool take_start();
void reset_request();
void frame(psprecomp::Runtime &runtime);
void filter_pad(gpu::PadState &pad);
void save_seen(const SavePreview &preview);
bool visible();
void draw();
}
