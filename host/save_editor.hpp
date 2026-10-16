#pragma once
#include "save_preview.hpp"
#include "save_data/savedata_store.hpp"
#include <array>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace portablekit::fu::editor {
inline constexpr std::size_t slot_stride=0x6b100, box_offset=0x3f88, box_slots=1000;
inline constexpr std::size_t zenny_offset=0x6a250, points_offset=0x6a248;
inline constexpr std::uint32_t currency_limit=9999999, stack_limit=99;
struct Item { std::uint16_t id{}; std::string name; };
struct GameData { SaveTable table{}; savedata::Block key{}; std::vector<Item> items; };
// Reads only the owner's supported ELF and ISO. No game tables or item names ship.
GameData read_game_data(const std::filesystem::path &elf, const std::filesystem::path &iso);
struct Hunter { HunterSummary summary; std::uint32_t points{}; std::size_t free_slots{}, capacity{}; };
std::size_t box_capacity(std::span<const std::uint8_t> plain,unsigned hunter);
struct Change {
    unsigned hunter{};
    std::optional<std::uint32_t> zenny, points;
    std::optional<std::uint16_t> item;
    std::uint32_t quantity{};
};
// Validates first, then fills the earliest empty slots. Refusal changes no bytes.
std::vector<std::size_t> apply_change(std::vector<std::uint8_t> &plain, const GameData &, const Change &);
class Session {
public:
    Session(const std::filesystem::path &save_file, GameData data);
    const std::array<Hunter,3> &hunters() const { return hunters_; }
    const GameData &game_data() const { return data_; }
    std::filesystem::path save_file() const;
    // Backup + staged verification + folder replacement. Refuses stale sessions.
    // Returns the retained backup folder; errors throw without partial edits.
    std::filesystem::path save(const Change &);
    std::filesystem::path save(const std::vector<Change> &);
private:
    void refresh();
    std::filesystem::path stick_;
    GameData data_;
    savedata::SaveFiles files_;
    savedata::SaveContents contents_;
    std::vector<std::uint8_t> plain_;
    std::array<Hunter,3> hunters_;
    std::string fingerprint_;
};
}
