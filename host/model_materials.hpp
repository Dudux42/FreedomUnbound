#pragma once
#include "hunter_model.hpp"
#include <memory>
#include <optional>
#include <string>
namespace psprecomp {class GuestMemory;}
namespace portablekit::gpu {struct TextureState;}
namespace portablekit::fu::model {
void retain_texture_source(Mesh &,const gpu::TextureState &,const psprecomp::GuestMemory &);
bool restore_texture_source(const Mesh &,gpu::TextureState &,psprecomp::GuestMemory &);
struct MaterialResult {
    std::uint64_t revision{},ticket{};
    Scene scene;
    unsigned replaced{};
    std::vector<std::filesystem::path> dependencies;
};
// One loader shared by all cards. No GUI or live guest state is used by its worker.
class MaterialLoader {
public:
    explicit MaterialLoader(std::string game_id);
    ~MaterialLoader();
    std::uint64_t configure(bool enabled,const std::filesystem::path &folder);
    void request(unsigned slot,std::uint64_t ticket,std::shared_ptr<const Scene> original);
    std::optional<MaterialResult> take(unsigned slot,std::uint64_t ticket);
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
