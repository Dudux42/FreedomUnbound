#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace portablekit::fu::model {
struct Vertex { float x{},y{},z{},nx{},ny{},nz{},u{},v{}; std::uint32_t color{0xffffffff}; };
struct Mesh {
    std::vector<Vertex> vertices; // triangle list, posed in preview camera space
    std::uint32_t width{},height{},wrap_u{},wrap_v{},texturing{},alpha_reference{};
    std::vector<std::uint32_t> pixels;
    // Original GE texture bytes/metadata for pack matching, never HD pixels.
    std::array<std::uint32_t,16> texture_state{};
    std::vector<std::uint8_t> texture_bytes,palette_bytes;
};
struct Scene { std::vector<Mesh> meshes; };
bool write(const std::filesystem::path &,const Scene &);
bool read(const std::filesystem::path &,Scene &);
// Independent orthographic camera, soft directional light and a depth buffer.
std::vector<std::uint32_t> render(const Scene &,unsigned width,unsigned height,float yaw);
}
