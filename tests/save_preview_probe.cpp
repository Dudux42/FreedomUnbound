#include "save_preview.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstdlib>
#include <vector>

// folder <ms0>: what the launcher shows. inner <blob>: decode a blob.
// record <blob> <ms0>: store the blob as the save and summarise it, as the
// game does when it saves. No key is involved anywhere. inner and record need
// the game's save table, which is not in the repo: MHFU_SAVE_TABLE names a local
// 256-byte copy of it.
int main(int argc, char **argv) {
    if(argc<3) return 1;
    const std::string mode=argv[1];
    portablekit::fu::SavePreview result;
    if(mode=="folder" || mode=="folder-details") result=portablekit::fu::read_save_preview(argv[2]);
    else {
        std::ifstream in(argv[2],std::ios::binary);
        std::vector<std::uint8_t> data(std::istreambuf_iterator<char>(in),{});
        portablekit::fu::SaveTable table{};
        const char *table_path=std::getenv("MHFU_SAVE_TABLE");
        if(!table_path) return 3;
        std::ifstream tin(table_path,std::ios::binary);
        if(!tin.read(reinterpret_cast<char *>(table.data()),std::streamsize(table.size()))) return 3;
        if(mode=="record" && argc==4) {
            const std::filesystem::path stick=argv[3];
            const auto folder=stick/"PSP"/"SAVEDATA"/"ULUS10391";
            std::filesystem::create_directories(folder);
            {
                std::ofstream out(folder/"MHP2NDG.BIN",std::ios::binary|std::ios::trunc);
                out.write(reinterpret_cast<const char *>(data.data()),std::streamsize(data.size()));
                if(!out) return 2;
            }
            portablekit::fu::record_save_preview(table,stick,"ULUS10391","MHP2NDG.BIN",data);
            return 0;
        }
        result=portablekit::fu::decode_save_preview(data,table);
    }
    using portablekit::fu::PreviewStatus;
    if(mode=="folder-details" || mode=="inner-details") {
        for(const auto &slot:result.slots) {
            std::cout<<slot.details_available<<'|'<<slot.pokke_points<<'|'<<slot.guild_points;
            for(const auto &gear:slot.equipment) std::cout<<'|'<<gear.occupied<<':'<<gear.type<<':'<<gear.id;
            std::cout<<'\n';
        }
        return 0;
    }
    std::cout<<(result.status==PreviewStatus::Ready?"ready":result.status==PreviewStatus::Missing?"missing":
                result.status==PreviewStatus::Pending?"pending":"unavailable")<<'\n';
    for(const auto &slot:result.slots)
        std::cout<<slot.occupied<<'|'<<slot.name<<'|'<<slot.hunter_rank<<'|'<<slot.zenny<<'|'
            <<slot.played_seconds<<'|'<<portablekit::fu::format_play_time(slot.played_seconds)<<'\n';
}
