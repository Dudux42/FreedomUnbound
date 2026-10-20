#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace portablekit::fu::mod_overlay {
// Register compiled-in providers before inspecting mods. ZIPs contain data only.
// All provider/lifecycle functions run on the UI thread; capture uses its own queue.
struct Provider {
    std::string id;
    std::vector<std::string> hooks;
    void (*enable)(const std::filesystem::path &folder){};
    void (*disable)(){};
    bool (*visible)(){};
    void (*draw)(){}; // Inside the existing ImGui frame, alongside the F12 toast.
    bool persistent{}; // Also visible over launcher menus and system dialogs.
    void (*settings)(){}; // Optional controls inside the in-game manager.
};
bool register_provider(Provider provider);
bool supports(const std::string &id);
bool is_active(const std::string &id);
bool has_hook(const std::string &id, const std::string &hook);
void activate(const std::string &id, const std::filesystem::path &folder);
void deactivate(const std::string &id);
bool visible(bool persistent = false);
void draw(bool persistent = false);
bool has_settings(const std::string &id);
void draw_settings(const std::string &id);
}
