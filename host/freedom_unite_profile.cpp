// The profile PortableKit asks for. Identity fields below were read from this
// disc on 2026-10-02. The encrypted executable was not decrypted: PortableKit
// has no key or 0x90-byte table for its tag. See DISC.md.

#include "profile.hpp"
#include "timing_probe.hpp"
#include "native_60.hpp"
#include "mods_page.hpp"
#include "bug_report.hpp"
#include "launcher_art.hpp"
#include "save_preview.hpp"
#include "save_portraits.hpp"
#include "direct_load.hpp"
#include "cheats.hpp"
#include "updater.hpp"
#include "whats_new.hpp"
#include "fu_text_mod.hpp"
#include "mod_manager.hpp"
#include "mod_overlay.hpp"
#include "input/bindings.hpp"
#include "damage_events.hpp"
#include "damage_numbers.hpp"
#include "gathering_nodes.hpp"
#include "gathering_map.hpp"
#include "monster_hp.hpp"
#include "modern_camera.hpp"
#include "shared_progression.hpp"
#include "key_quests.hpp"
#include "return_timer.hpp"
#include "buff_icons.hpp"
#include "equipment_wishlist.hpp"
#include "controller_prompts.hpp"
#include "native_prompts.hpp"
#include "wishlist_scene.hpp"
#include "mods_used.hpp"

#include <cstdlib>
#include "psprecomp/guest_memory.hpp"
#include "gpu/ge_state.hpp"

namespace portablekit {

namespace {

std::uint64_t texture_scene(const psprecomp::GuestMemory &memory) {
    // The verified village/lobby overlay shares one warmup group. It does not
    // expose the quest actor layout, so never interpret those fields here.
    if (memory.load32(0x09AA5770u) == 0x27BDFDC0u &&
        memory.load32(0x09AA49F8u) == 0x27BDFFF0u &&
        memory.load32(0x09AA5D90u) == 0x00A08021u) return std::uint64_t{1} << 32u;
    // Only interpret actor/map fields while the verified USA quest overlay is present.
    if (memory.load32(0x08845CE4u) != 0x3C020894u ||
        memory.load32(0x09A6F490u) != 0x27BDFFF0u) return 0;
    const auto state = memory.load32(0x089CC558u);
    const auto hunter = memory.load32(0x099998F4u);
    if (state < 0x08800000u || state > 0x0A000000u - 0x6AF11u || (state & 3u) ||
        hunter < 0x08800000u || hunter > 0x0A000000u - 0x29Cu || (hunter & 3u) ||
        !memory.contains(state, 0x6AF11u) || !memory.contains(hunter, 0x29Cu)) return 0;
    const auto map = memory.load8(state + 0x6AF10u);
    const auto phase = memory.load8(state + 46u);
    if (phase != 2u && phase != 3u) return 0;
    const auto area = memory.load16(hunter + 0x29Au);
    if (area > 255u) return 0;
    return 1u + (std::uint64_t(map) << 16u) + area;
}

// BOOT.BIN's zero-sized overlay sections and type-0 program headers describe
// these slots. Each maximum memsz ends at the next start; debug.elf occupies
// the last 0x80 bytes. The final address is the end sentinel, not another slot.
constexpr std::uint32_t kOverlaySlots[] = {
    0x09A5F300u, 0x09C19100u, 0x09D1A280u,
    0x09D63100u, 0x09D65500u, 0x09D65580u,
};

std::span<const std::uint32_t> overlay_slots() {
    // Keep an off switch for comparison with the working prototype.
    const char *disabled = std::getenv("MHFU_DISABLE_OVERLAY_SLOTS");
    if (disabled != nullptr && disabled[0] == '1' && disabled[1] == '\0') return {};
    return kOverlaySlots;
}

void install_game_hooks(psprecomp::Runtime &runtime, const psprecomp::Elf32Image &image) {
    fu::damage_events::clear();
    fu::gathering_nodes::clear();
    fu::monster_hp::clear();
    fu::load_cheat_preferences();
    fu::initialize_cheats(runtime);
    fu::install_timing_probe(runtime, image);
    // Text mods: read text\NNNN.txt and font\map.txt once, before the game loads DATA.BIN.
    fu::text::load_enabled_text_mods();
}

// The game's save folders, as directory names under PSP/SAVEDATA (not the
// ms0: prefix). PortableKit builds <ms0>/PSP/SAVEDATA/<gameName><saveName>/.
// The save screen accepts a save only when the SAVEDATA_DIRECTORY in its
// PARAM.SFO is listed here (is_game_save_name), and export and backup carry
// only folders listed here.
//   ULUS10391     the hunter's save: AUTOSAVE/AUTOLOAD with gameName ULUS10391
//                 and an empty saveName, data file MHP2NDG.BIN. Its PARAM.SFO
//                 has SAVEDATA_DIRECTORY ULUS10391 (read 2026-10-05).
//   ULUS10391DAT  recorded from the executable. The numbered path
//                 ULUS10391DAT\%08d is this same directory with %08d inside
//                 it, not a second folder.
// save_game_name is the disc id the utility passes as gameName.
const SaveFolder kSaveFolders[] = {
    {"ULUS10391", "Game data", true, "launcher.home.game_data"},
    {"ULUS10391DAT", "ULUS10391DAT", true},
};


// FU Saves, item 11: the enabled mods, as mods-used.txt records them.
std::vector<fu::mods_used::Mod> current_mods() {
    std::vector<fu::mods_used::Mod> list;
    for (const fu::mods::InstalledMod &mod : fu::mods::enabled())
        list.push_back({mod.info.id, mod.info.version, mod.info.name, mod.info.affects_save});
    return list;
}

// The save-format table is the game's own data, so it is never shipped. While
// the game runs it sits in guest memory (USA build), and is read from there
// each time a save is written or loaded. If it doesn't look right, nothing is
// summarised and the launcher keeps showing "Preview pending".
constexpr std::uint32_t kSaveTableAddress = 0x089B2DB4u;
void record_save_from_guest(psprecomp::GuestMemory &memory, const std::filesystem::path &memory_stick,
                            const std::string &folder, const std::string &file_name,
                            std::span<const std::uint8_t> data) {
    if (folder != "ULUS10391" || file_name != "MHP2NDG.BIN") return;
    fu::SaveTable table{};
    for (std::uint32_t i = 0; i < table.size(); ++i) table[i] = memory.load8(kSaveTableAddress + i);
    if (!fu::valid_save_table(table)) return;
    fu::record_save_preview(table, memory_stick, folder, file_name, data);
    const auto preview=fu::decode_save_preview(data,table);
    fu::portrait_save_seen(preview);
    fu::direct_load::save_seen(preview);
}

// English for a string a text mod translated (fu_text_mod.cpp); otherwise the same text.
const char *original_text(const char *shown) { return fu::text::original_text(shown); }

bool overlays_visible() {
    // Restore on the UI thread, before the first gameplay overlay frame.
    static const bool restored = [] { fu::mods::restore_overlays(); return true; }();
    (void)restored;
    return fu::mod_overlay::visible();
}
void overlays_draw() { fu::mod_overlay::draw(); }
// FU Controls: F7 belongs to the Equipment Wishlist mod. Only while the mod is
// on does it open the panel and count as reserved; otherwise the player's own
// binding works. Follows enable/disable at runtime (mod_overlay activate/deactivate).
[[maybe_unused]] const bool mod_keys_claimed = [] {
    portablekit::input::claim_key(64 /* F7 */, [] { return fu::mod_overlay::is_active("equipment-wishlist"); });
    return true;
}();
bool transform_game_draw(const gpu::DrawCall &call,const psprecomp::GuestMemory &memory,std::vector<gpu::DrawCall> &out) {
    fu::wishlist::observe_scene(call,memory);
    return fu::controller_prompts::transform_native(call,memory,out);
}
bool persistent_overlays_visible() {
    (void)overlays_visible();
    return fu::mod_overlay::visible(true) || fu::direct_load::visible();
}
void persistent_overlays_draw() { fu::mod_overlay::draw(true); fu::direct_load::draw(); }

// FU Saves: the occupied slots a backup's copy of save-preview/ULUS10391.txt
// names, for the Saves screen's restore list. Ranks and play time only.
int backup_summary(const std::filesystem::path &summary_file, BackupSlot *slots, int max) {
    const fu::SavePreview preview = fu::read_preview_file(summary_file);
    if (preview.status != fu::PreviewStatus::Ready) return 0;
    int count = 0;
    for (const fu::HunterSummary &slot : preview.slots)
        if (slot.occupied && count < max) slots[count++] = {slot.hunter_rank, slot.played_seconds};
    return count;
}

// FU Saves, item 11: the profile's save_data_seen hook. record_save_from_guest
// is left as it is; the mods this save was last written or loaded with are
// recorded after it, for the launcher's warning before Play.
void on_save_data_seen(psprecomp::GuestMemory &memory, const std::filesystem::path &memory_stick,
                       const std::string &folder, const std::string &file_name, std::span<const std::uint8_t> data) {
    record_save_from_guest(memory, memory_stick, folder, file_name, data);
    if (folder != "ULUS10391" || file_name != "MHP2NDG.BIN") return;
    fu::mods_used::write(fu::mods_used::file_for(memory_stick), current_mods());
}

// FU Saves, item 11: the save-changing mods added or removed since the save was
// last used. Nothing to warn about without a save.
bool save_mods_changed(const std::filesystem::path &memory_stick, std::vector<std::string> &added,
                       std::vector<std::string> &removed) {
    std::error_code ec;
    if (!std::filesystem::is_regular_file(memory_stick / "PSP" / "SAVEDATA" / "ULUS10391" / "MHP2NDG.BIN", ec))
        return false;
    const fu::mods_used::Difference diff =
        fu::mods_used::compare(fu::mods_used::read(fu::mods_used::file_for(memory_stick)), current_mods());
    added = diff.added;
    removed = diff.removed;
    return diff.any();
}

} // namespace

const GameProfile &game() {
    fu::damage_numbers::register_renderer();
    fu::gathering_nodes::register_renderer();
    fu::gathering_map::register_renderer();
    fu::monster_hp::register_renderer();
    fu::modern_camera::register_mod();
    fu::shared_progression::register_mod();
    fu::key_quests::register_renderer();
    fu::return_timer::register_renderer();
    fu::buff_icons::register_renderer();
    fu::wishlist::register_renderer();
    fu::controller_prompts::register_renderer();
    static const GameProfile profile{
        .app_name = "FreedomUnbound",
        .project_name = "Freedom Unbound",
        .env_prefix = "MHFU",
        .data_organization = "FreedomUnite",
#if MHFU_TEST_DATA
        .data_application = "FreedomUnite-Test",
#else
        .data_application = "FreedomUnite",
#endif

        // PSP_GAME/PARAM.SFO on this disc. DISC_ID has no hyphen.
        .disc_id = "ULUS10391",
        .disc_id_display = "ULUS10391",
        .game_title = "MONSTER HUNTER FREEDOM UNITE™",
        // The installer reads the plain BOOT.BIN (matches executable_sha256), so no decryption key is needed.
        .executable_path_on_disc = "PSP_GAME/SYSDIR/BOOT.BIN",
        .param_sfo_path_on_disc = "PSP_GAME/PARAM.SFO",
        .disc_source_note = "dump it from your own UMD with a PSP",
        // SHA-256 of PSP_GAME/SYSDIR/EBOOT.BIN as stored on the disc.
        .encrypted_executable_sha256 = "3354adb3ee40821f230c4055b9bf7cc5d4c864e42abb74ce8ac022933f9697a7",
        // SHA-256 of the plain BOOT.BIN, not a decrypted EBOOT.
        .executable_sha256 = "3070954e2dc88fd0b0836c0c1dc526531aca4a213b0f18806e324963efb514f4",
        // Little-endian word at offset 0xD0 of EBOOT.BIN.
        .decryption_tag = 0xC0CB167Cu,
        .decryption_key = {},

        // PT_LOAD vaddr of the on-disc BOOT.BIN (ELF type ET_EXEC, not a PRX).
        // The loader uses that vaddr directly. Not checked against a decrypt.
        .load_base = 0x08804000u,
        // required_ram_size for that PT_LOAD: the image ends inside 32 MiB.
        .guest_ram_bytes = 0x02000000u,
        .boot_path = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN",
        .overlay_slots = overlay_slots(),

        .save_game_name = "ULUS10391",
        .save_folders = kSaveFolders,

        // What this game calls the player. The existing menu reads these.
        .player_name_label = "Hunter name",
        .default_player_name = "Hunter",

        .adhoc_product_code = "ULUS10391",

        // Analog triggers belong to host mods; PSP L/R stay on the bumpers.
        .trigger_profiles = {},
        // One stick, and the camera turns with the D-pad: the right stick presses it.
        .right_stick_dpad_default = true,

        .patch_loaded_image = install_game_hooks,
        .camera = fu::portrait_camera_driver(),
        // Fill: the renderer narrows the game's perspective projections to the
        // window's shape (gpu::set_view_aspect), so no guest memory is touched.
        .view_aspect_frame = [](psprecomp::Runtime &, float aspect) { gpu::set_view_aspect(aspect); },
        .frame_vblanks = fu::native_60_requested() ? 1u : 2u,
        .texture_scene = texture_scene,

        // USA release: English, confirm with cross. Not a Japanese console.
        .system_language = 1u,
        .confirm_button = 1u,

        // Open on the launcher's home screen; the game boots when Play is chosen.
        .launcher_home = true,
        .launcher_mods_page = fu::show_mods_page,
        .game_mods_frame = fu::game_mods_frame,
        .game_wishlist_frame = fu::wishlist::panel,
        .launcher_cheats_page = fu::show_cheats_page,
        .menu_cheats_rows = fu::draw_cheat_rows,
        .game_overlay_visible = overlays_visible,
        .game_overlay_draw = overlays_draw,
        .persistent_overlay_visible = persistent_overlays_visible,
        .persistent_overlay_draw = persistent_overlays_draw,
        .launcher_background = fu::draw_launcher_background,
        .launcher_logo = fu::draw_launcher_logo,
        .launcher_save_preview = fu::draw_save_preview,
        .launcher_start_requested = fu::direct_load::take_start,
        .filter_pad = fu::direct_load::filter_pad,
        .launcher_update_button = fu::draw_update_button,
        .launcher_banner = fu::draw_update_banner,
        .launcher_report_page = fu::show_bug_report_page,
        .launcher_whats_new_page = fu::show_whats_new_page,
        .launcher_whats_new_due = fu::whats_new_due,
        .original_text = original_text,
        .save_data_seen = on_save_data_seen,

        // FU Saves: keep the newest 5 copies of the save, each with its summary.
        .auto_backup_keep = 5,
        .save_summary_file = "save-preview/ULUS10391.txt",
        .backup_summary = backup_summary,
        .save_data_file = "MHP2NDG.BIN",
        .save_mods_changed = save_mods_changed,
        .transform_draw = transform_game_draw,
    };
    return profile;
}

} // namespace portablekit
