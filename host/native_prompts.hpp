#pragma once
#include <vector>
namespace psprecomp {class GuestMemory;}
namespace portablekit::gpu {struct DrawCall;}
namespace portablekit::fu::controller_prompts {
bool transform_native(const gpu::DrawCall &,const psprecomp::GuestMemory &,std::vector<gpu::DrawCall> &);
void enable_native(bool);
}
