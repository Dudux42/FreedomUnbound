#pragma once

// Mods as the player installs them: a .zip dropped on the launcher's Mods
// page. A zip carries a mod.json at its top (or in its one top folder); see
// MODDING.md for the full format:
//
//   { "id": "luis-ptbr", "name": "Portugues (Brasil)", "version": "1.0.0",
//     "author": "Luis Eduardo", "type": "text", "api_version": 1,
//     "language": "pt-BR", "description": "...", "requires": [],
//     "conflicts": [], "affects_save": false, "load_order": 0 }
//
// Without one, the zip's file name is the name and id, and the type is found
// from what it holds (a textures.ini makes it a texture pack). Older packs
// that say "textures" or have no type at all still load as texture mods.
// Each mod is unpacked into <data>/mods/<id>; mods/enabled.txt lists the ones
// switched on, one id per line, in load order (first loads first, later ones
// win an overlap).
//
// This build supports data mods and registered native providers:
//   texture  used in place as the game's texture pack
//            (video.texture_pack_folder); one texture mod is on at a time.
//   text     strings (text/), host strings (text/host.txt) and font (font/)
//            for one language; one text mod per language is on at a time.
//            Its font replaces only the font textures, on top of any texture
//            mod. Other bots read these through enabled_text_mods().
// overlay requires a registered native provider; ZIPs supply its data only.
// camera and qol also require a compiled native provider. Audio is refused.
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace portablekit::fu::mods {

// The mod.json api_version this build understands. A mod asking for a newer
// one is refused.
inline constexpr int kApiVersion = 1;

struct ModInfo {
    std::string id, name, version, author, description, type, game_id;
    std::string language;                 // text mods, e.g. "pt-BR"
    std::vector<std::string> required_hooks;   // hooks, e.g. "overlay", "camera_hook", "sound_api"
    std::vector<std::string> conflicts;   // ids of mods that shouldn't be on with this one
    int api_version{kApiVersion};
    bool has_api_version{};               // mod.json named it (legacy texture packs don't)
    bool affects_save{};
    int load_order{};                     // where it goes in enabled.txt when switched on
    bool has_manifest{};
};

// What the install pop-up shows for a dropped file.
struct ModPreview {
    std::filesystem::path zip;
    ModInfo info;
    std::size_t files{};
    std::uint64_t bytes{};
    std::string installed_version;  // a mod with this id is installed already
    bool replaces{};
    std::string problem;            // why it cannot be installed; empty when it can
    [[nodiscard]] bool ok() const noexcept { return problem.empty(); }
};

struct InstalledMod {
    ModInfo info;
    std::filesystem::path folder;   // mods/<id>
    std::filesystem::path content;  // texture: the pack folder inside it; text: <folder>/text
    bool enabled{};
    int position{-1};               // place in enabled.txt; -1 when off
    std::string problem;            // why it cannot be used; empty when it can
    std::vector<std::string> warnings;  // e.g. an enabled mod it conflicts with
};

// An enabled text mod, for the string hook, the font loader and host.txt.
struct TextMod {
    std::string id, language;
    std::filesystem::path folder;   // mods/<id>
    std::filesystem::path text;     // mods/<id>/text (may not exist)
    std::filesystem::path font;     // mods/<id>/font (may not exist)
    std::filesystem::path host;     // mods/<id>/text/host.txt (may not exist)
};

struct InstallProgress {
    std::atomic<std::uint64_t> bytes{0};
    std::atomic<std::uint64_t> total{0};
};

[[nodiscard]] std::filesystem::path mods_root();
// The name the Mods page shows for a type ("Texture", "Text"...).
[[nodiscard]] std::string type_label(const std::string &type);
[[nodiscard]] ModPreview inspect(const std::filesystem::path &dropped);
// Unpacks a checked zip into mods/<id>, replacing an older copy of the same
// mod (its on/off state is kept). Runs on any thread.
[[nodiscard]] bool install(const ModPreview &preview, InstallProgress &progress, std::string &error);
// Every installed mod, grouped by type, enabled ones in load order first.
[[nodiscard]] std::vector<InstalledMod> installed();
// The enabled mods in load order (what FU Saves lists in mods-used.txt).
[[nodiscard]] std::vector<InstalledMod> enabled();
// The enabled text mods in load order, at most one per language.
[[nodiscard]] std::vector<TextMod> enabled_text_mods();
// Restore validated native overlay providers at game startup (UI/boot thread).
void restore_overlays();
// Switches a mod on or off and applies it. True when what the renderer draws
// changed (texture pack or a text mod's font), so it should reload. notice,
// when given, is set to a message such as which mod was switched off.
bool set_enabled(const InstalledMod &mod, bool on, std::string &error, std::string *notice = nullptr);
// Moves an enabled mod up (-1) or down (+1) in enabled.txt.
bool move(const InstalledMod &mod, int delta, std::string &error);
bool remove(const InstalledMod &mod, std::string &error);

} // namespace portablekit::fu::mods
