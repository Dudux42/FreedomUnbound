#include "gpu/overlay_projection.hpp"
#include <stdexcept>
#include <limits>
void check(bool ok) { if (!ok) throw std::runtime_error("world projection regression"); }
int main() {
    using namespace portablekit::gpu;
    const std::array<float, 16> identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
    auto projection = identity;
    projection[10] = 0; projection[11] = -1; projection[15] = 0;
    ViewportState vp{};
    vp.x_scale = 240; vp.y_scale = -136;
    vp.x_offset = 2288; vp.y_offset = 2184;
    vp.offset_x = 2048; vp.offset_y = 2048;
    auto p = project_overlay_point({0,0,-2}, identity, projection, vp);
    check(p && (*p)[0] == 0.5f && (*p)[1] == 0.5f);
    p = project_overlay_point({1,1,-2}, identity, projection, vp);
    check(p && (*p)[0] == 0.75f && (*p)[1] == 0.25f);
    auto moved_camera = identity; moved_camera[12] = -1;
    p = project_overlay_point({1,0,-2}, moved_camera, projection, vp);
    check(p && (*p)[0] == 0.5f);
    check(!project_overlay_point({0,0,2}, identity, projection, vp));
    check(!project_overlay_point({20,0,-2}, identity, projection, vp));
    check(!project_overlay_point({0,0,0}, identity, projection, vp));
    check(!project_overlay_point({std::numeric_limits<float>::quiet_NaN(),0,-2}, identity, projection, vp));
}
