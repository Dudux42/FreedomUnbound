#include "mod_overlay.hpp"
#include <algorithm>
#include <map>
#include <utility>

namespace portablekit::fu::mod_overlay {
namespace {
struct Entry { Provider provider; bool active{}; };
// Profiles may register during another translation unit's static initialization.
std::map<std::string, Entry> &providers() {
    static std::map<std::string, Entry> entries;
    return entries;
}
}
bool register_provider(Provider provider) {
    if (provider.id.empty() || !provider.enable || !provider.disable || !provider.visible || !provider.draw)
        return false;
    const auto id = provider.id;
    return providers().emplace(id, Entry{std::move(provider)}).second;
}
bool supports(const std::string &id) { return providers().count(id) != 0; }
bool is_active(const std::string &id) {
    const auto it = providers().find(id);
    return it != providers().end() && it->second.active;
}
bool has_hook(const std::string &id, const std::string &hook) {
    const auto it = providers().find(id);
    if (it == providers().end()) return false;
    if (hook == "overlay_draw") return true;
    const auto &hooks = it->second.provider.hooks;
    return std::find(hooks.begin(), hooks.end(), hook) != hooks.end();
}
void activate(const std::string &id, const std::filesystem::path &folder) {
    const auto it = providers().find(id);
    if (it == providers().end()) return;
    if (it->second.active) it->second.provider.disable();
    it->second.provider.enable(folder);
    it->second.active = true;
}
void deactivate(const std::string &id) {
    const auto it = providers().find(id);
    if (it == providers().end() || !it->second.active) return;
    it->second.active = false;
    it->second.provider.disable();
}
bool visible(bool persistent) {
    for (const auto &[id, entry] : providers())
        if (entry.active && entry.provider.persistent == persistent && entry.provider.visible()) return true;
    return false;
}
void draw(bool persistent) {
    for (const auto &[id, entry] : providers())
        if (entry.active && entry.provider.persistent == persistent && entry.provider.visible()) entry.provider.draw();
}
}
