#pragma once
#include "controller_prompts.hpp"
#include "gpu/ge_state.hpp"
#include "input/bindings.hpp"
#include <functional>
#include <string>
namespace portablekit::fu::controller_prompts {
bool replace_native_draw(const gpu::DrawCall &,unsigned,
    const std::function<std::string(input::Action)> &,std::vector<gpu::DrawCall> &,Family=Family::Generic);
}
