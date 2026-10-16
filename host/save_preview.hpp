#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace portablekit::fu {
struct EquippedItem {
    bool occupied=false;
    std::uint32_t type=0, id=0;
};
struct HunterSummary {
    bool occupied = false;
    std::string name;
    std::uint32_t zenny = 0;
    std::uint32_t played_seconds = 0;
    std::uint32_t hunter_rank = 0;
    std::string appearance_key;
    bool details_available=false;
    std::uint32_t pokke_points=0, guild_points=0;
    std::array<EquippedItem,6> equipment{};
};
// Pending: there is a save, but the game has not summarised it yet.
enum class PreviewStatus { Ready, Missing, Pending, Unavailable };
struct SavePreview {
    PreviewStatus status = PreviewStatus::Unavailable;
    std::array<HunterSummary, 3> slots;
};
// The game's 256-byte save substitution table. It is game data, so it never
// ships: the profile reads it from guest memory while the game is running.
using SaveTable = std::array<std::uint8_t, 256>;
// True when the table is a permutation of 0..255, as the real one is.
bool valid_save_table(const SaveTable &table);
// Inner save codec shared with the standalone editor. Encoding refreshes the
// salted checksum while retaining all other bytes and the caller's seed.
std::optional<std::vector<std::uint8_t>> decode_save_data(std::span<const std::uint8_t>, const SaveTable &);
std::optional<std::vector<std::uint8_t>> encode_save_data(std::span<const std::uint8_t>, const SaveTable &, std::uint32_t seed);
// The input is the PSP-decrypted data, still encoded by the game.
SavePreview decode_save_preview(std::span<const std::uint8_t> data, const SaveTable &table);
// Reads the summary the game left; never opens or decrypts the save itself.
SavePreview read_save_preview(const std::filesystem::path &memory_stick);
std::filesystem::path preview_file(const std::filesystem::path &memory_stick);
// A summary file read as it is, without checking it against a save: a copy kept
// with an automatic save backup (FU Saves).
SavePreview read_preview_file(const std::filesystem::path &file);
// The slot lines after the magic and stamp lines.
SavePreview read_slots(std::istream &in);
// Profile hook: called with the decrypted data of every save written or loaded.
void record_save_preview(const SaveTable &table, const std::filesystem::path &memory_stick, const std::string &folder,
                         const std::string &file_name, std::span<const std::uint8_t> data);
std::string format_play_time(std::uint32_t seconds);
void draw_save_preview(float x, float y, float width, float height, bool refresh);
}
