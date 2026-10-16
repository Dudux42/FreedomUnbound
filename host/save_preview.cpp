#include "save_preview.hpp"
#include "psprecomp/sha256.hpp"

#include <algorithm>
#include <bit>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace portablekit::fu {
namespace {
constexpr std::size_t kEncodedSize = 0x16a100;
constexpr std::size_t kHashOffset = 0x16a0e8;
constexpr std::size_t kSignedSize = 0x16a0dc;
constexpr std::size_t kSlotStride = 0x6b100;
std::uint32_t le32(std::span<const std::uint8_t> data, std::size_t at) {
    return data[at] | (std::uint32_t(data[at+1]) << 8) |
        (std::uint32_t(data[at+2]) << 16) | (std::uint32_t(data[at+3]) << 24);
}
// Standard SHA-1, used only to check the game's existing save checksum.
std::array<std::uint8_t, 20> digest(std::span<const std::uint8_t> data) {
    std::vector<std::uint8_t> padded(data.begin(), data.end());
    const std::uint64_t bits = data.size() * 8ull;
    padded.push_back(0x80);
    while (padded.size() % 64 != 56) padded.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8) padded.push_back(std::uint8_t(bits >> shift));
    std::array<std::uint32_t, 5> state{0x67452301,0xefcdab89,0x98badcfe,0x10325476,0xc3d2e1f0};
    for (std::size_t block = 0; block < padded.size(); block += 64) {
        std::array<std::uint32_t, 80> words{};
        for (int i = 0; i < 16; ++i) {
            const auto at = block + i*4;
            words[i] = (std::uint32_t(padded[at]) << 24) | (std::uint32_t(padded[at+1]) << 16) |
                (std::uint32_t(padded[at+2]) << 8) | padded[at+3];
        }
        for (int i = 16; i < 80; ++i)
            words[i] = std::rotl(words[i-3] ^ words[i-8] ^ words[i-14] ^ words[i-16], 1);
        auto [a,b,c,d,e] = state;
        for (int i = 0; i < 80; ++i) {
            const auto f = i < 20 ? ((b & c) | (~b & d)) : i < 40 ? (b ^ c ^ d) :
                i < 60 ? ((b & c) | (b & d) | (c & d)) : (b ^ c ^ d);
            const std::uint32_t k = i < 20 ? 0x5a827999 : i < 40 ? 0x6ed9eba1 :
                i < 60 ? 0x8f1bbcdc : 0xca62c1d6;
            const auto next = std::rotl(a,5) + f + e + k + words[i];
            e=d; d=c; c=std::rotl(b,30); b=a; a=next;
        }
        state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d; state[4]+=e;
    }
    std::array<std::uint8_t,20> result{};
    for (int i=0;i<20;++i) result[i]=std::uint8_t(state[i/4] >> (24-(i%4)*8));
    return result;
}
void append_utf8(std::string &out, std::uint32_t cp) {
    if(cp<0x80) out+=char(cp);
    else if(cp<0x800) { out+=char(0xc0|(cp>>6)); out+=char(0x80|(cp&63)); }
    else if(cp<0x10000) { out+=char(0xe0|(cp>>12)); out+=char(0x80|((cp>>6)&63)); out+=char(0x80|(cp&63)); }
    else { out+=char(0xf0|(cp>>18)); out+=char(0x80|((cp>>12)&63)); out+=char(0x80|((cp>>6)&63)); out+=char(0x80|(cp&63)); }
}
std::string hunter_name(std::span<const std::uint8_t> data, std::size_t at) {
    std::string out;
    const auto unit = [&](int i) { return std::uint32_t(data[at+i*2]) | (std::uint32_t(data[at+i*2+1])<<8); };
    for(int i=0;i<8;++i) {
        auto cp=unit(i);
        if(!cp) break;
        if(cp>=0xd800 && cp<=0xdbff) {
            if(i+1<8 && unit(i+1)>=0xdc00 && unit(i+1)<=0xdfff) {
                cp=0x10000+((cp-0xd800)<<10)+(unit(i+1)-0xdc00); ++i;
            } else cp=0xfffd;
        } else if(cp>=0xdc00 && cp<=0xdfff) cp=0xfffd;
        if(cp<32 || cp==127) cp=0xfffd;
        append_utf8(out,cp);
    }
    return out;
}
}
bool valid_save_table(const SaveTable &table) {
    std::array<bool,256> seen{};
    for(const auto v:table) { if(seen[v]) return false; seen[v]=true; }
    return true;
}
std::optional<std::vector<std::uint8_t>> decode_save_data(std::span<const std::uint8_t> data, const SaveTable &kSaveDecode) {
    if(data.size()!=kEncodedSize || !valid_save_table(kSaveDecode)) return std::nullopt;
    std::uint32_t seed=0;
    for(int b=0;b<4;++b) seed |= std::uint32_t(kSaveDecode[kSaveDecode[data[data.size()-4+b]]]) << (b*8);
    std::uint32_t hi=seed>>16, lo=seed&0xffff;
    if(!hi) hi=0xdfa3;
    if(!lo) lo=0x215f;
    std::vector<std::uint8_t> plain(data.size()-4);
    for(std::size_t at=0;at<plain.size();at+=4) {
        hi=(hi*0xdfa3)%0xffef; lo=(lo*0x215f)%0xff8f;
        const std::uint32_t mask=(hi<<16)|lo;
        for(int b=0;b<4;++b) plain[at+b]=kSaveDecode[kSaveDecode[data[at+b]] ^ std::uint8_t(mask>>(b*8))];
    }
    constexpr char salt[]="3Nc94Hq1zOLh8d62Sb69";
    std::vector<std::uint8_t> signed_data(plain.begin(),plain.begin()+kSignedSize);
    signed_data.insert(signed_data.end(),salt,salt+20);
    const auto checksum=digest(signed_data);
    if(!std::equal(checksum.begin(),checksum.end(),plain.begin()+kHashOffset)) return std::nullopt;
    return plain;
}
std::optional<std::vector<std::uint8_t>> encode_save_data(std::span<const std::uint8_t> data, const SaveTable &table, std::uint32_t seed) {
    if(data.size()!=kEncodedSize-4 || !valid_save_table(table)) return std::nullopt;
    std::vector<std::uint8_t> plain(data.begin(),data.end());
    constexpr char salt[]="3Nc94Hq1zOLh8d62Sb69";
    std::vector<std::uint8_t> signed_data(plain.begin(),plain.begin()+kSignedSize);
    signed_data.insert(signed_data.end(),salt,salt+20);
    const auto checksum=digest(signed_data);
    std::copy(checksum.begin(),checksum.end(),plain.begin()+kHashOffset);
    SaveTable inverse{};
    for(unsigned i=0;i<256;++i) inverse[table[i]]=static_cast<std::uint8_t>(i);
    std::vector<std::uint8_t> encoded(kEncodedSize);
    std::uint32_t hi=seed>>16,lo=seed&0xffff;
    if(!hi) hi=0xdfa3;
    if(!lo) lo=0x215f;
    for(std::size_t at=0;at<plain.size();at+=4) {
        hi=(hi*0xdfa3)%0xffef; lo=(lo*0x215f)%0xff8f;
        const auto mask=(hi<<16)|lo;
        for(unsigned b=0;b<4;++b)
            encoded[at+b]=inverse[inverse[plain[at+b]] ^ static_cast<std::uint8_t>(mask>>(b*8))];
    }
    for(unsigned b=0;b<4;++b) encoded[plain.size()+b]=inverse[inverse[static_cast<std::uint8_t>(seed>>(b*8))]];
    return encoded;
}
SavePreview decode_save_preview(std::span<const std::uint8_t> data, const SaveTable &kSaveDecode) {
    auto decoded=decode_save_data(data,kSaveDecode);
    if(!decoded) return {};
    const auto &plain=*decoded;
    SavePreview result;
    result.status=PreviewStatus::Ready;
    for(std::size_t i=0;i<3;++i) {
        const auto base=i*kSlotStride;
        const auto present=plain[base+0x1012];
        if(present>2) return {};
        if(!present) continue;
        auto &slot=result.slots[i];
        slot.name=hunter_name(plain,base+0x1000);
        if(slot.name.empty()) return {};
        slot.occupied=true;
        slot.played_seconds=le32(plain,base+0x4fc8);
        slot.zenny=le32(plain,base+0x6a250);
        slot.pokke_points=le32(plain,base+0x6a248);
        slot.guild_points=le32(plain,base+0x6a24c);
        slot.details_available=true;
        // Save order is weapon, feet, head, torso, braces, waist. Present the
        // same top-to-bottom order as the game's Status equipment page.
        constexpr unsigned records[]={0,2,3,4,5,1};
        for(unsigned e=0;e<6;++e) {
            const auto at=base+0x1020+records[e]*12;
            slot.equipment[e]={plain[at]!=0,plain[at+1],unsigned(plain[at+2])|(unsigned(plain[at+3])<<8)};
        }
        const auto rank=plain[base+0x501c];
        if(rank>8) return {};
        slot.hunter_rank=std::uint32_t(rank)+1;
        // Identity, sex, face, hair and the six equipped 12-byte records.
        slot.appearance_key=psprecomp::sha256_bytes(std::span(plain).subspan(base+0x1000,0x68));
    }
    return result;
}
namespace {
// The launcher never decrypts the save, so no key ships with it. Each time the
// game saves or loads, the recomp is handed the decrypted data and writes this
// small summary beside the memory stick; the launcher only reads it back.
constexpr const char *kPreviewMagic="FUPREVIEW 1";
bool summary_line(std::istream &in,std::string &line) {
    if(!std::getline(in,line)) return false;
    if(!line.empty() && line.back()=='\r') line.pop_back();
    return true;
}
std::filesystem::path stick_root(const std::filesystem::path &memory_stick) {
    auto root=memory_stick.lexically_normal();
    if(!root.has_filename()) root=root.parent_path();
    return root;
}
std::filesystem::path save_file(const std::filesystem::path &memory_stick) {
    return stick_root(memory_stick)/"PSP"/"SAVEDATA"/"ULUS10391"/"MHP2NDG.BIN";
}
std::string save_stamp(const std::filesystem::path &memory_stick, std::error_code &ec) {
    const auto stamp=std::filesystem::last_write_time(save_file(memory_stick),ec);
    return ec ? std::string{} : std::to_string(stamp.time_since_epoch().count());
}
}
std::filesystem::path preview_file(const std::filesystem::path &memory_stick) {
    return stick_root(memory_stick).parent_path()/"save-preview"/"ULUS10391.txt";
}
void record_save_preview(const SaveTable &table, const std::filesystem::path &memory_stick, const std::string &folder,
                         const std::string &file_name, std::span<const std::uint8_t> data) {
    if(folder!="ULUS10391" || file_name!="MHP2NDG.BIN") return;
    const auto preview=decode_save_preview(data,table);
    if(preview.status!=PreviewStatus::Ready) return;
    std::error_code ec;
    const auto stamp=save_stamp(memory_stick,ec);
    if(ec) return;
    const auto target=preview_file(memory_stick);
    std::filesystem::create_directories(target.parent_path(),ec);
    if(ec) return;
    auto temp=target; temp+=".tmp";
    {
        std::ofstream out(temp,std::ios::binary|std::ios::trunc);
        if(!out) return;
        out<<kPreviewMagic<<'\n'<<"stamp "<<stamp<<'\n';
        for(std::size_t i=0;i<preview.slots.size();++i) {
            const auto &slot=preview.slots[i];
            out<<"slot "<<i<<' '<<(slot.occupied?1:0)<<' '<<slot.hunter_rank<<' '<<slot.zenny<<' '
               <<slot.played_seconds<<' '<<slot.name<<'\n';
        }
        for(std::size_t i=0;i<preview.slots.size();++i)
            if(preview.slots[i].occupied) out<<"appearance "<<i<<' '<<preview.slots[i].appearance_key<<'\n';
        for(std::size_t i=0;i<preview.slots.size();++i) if(preview.slots[i].occupied) {
            const auto &slot=preview.slots[i];
            out<<"details "<<i<<' '<<slot.pokke_points<<' '<<slot.guild_points<<'\n';
            for(unsigned e=0;e<6;++e) {
                const auto &item=slot.equipment[e];
                out<<"gear "<<i<<' '<<e<<' '<<item.occupied<<' '<<item.type<<' '<<item.id<<'\n';
            }
        }
        out.flush();
        if(!out) { out.close(); std::filesystem::remove(temp,ec); return; }
    }
    std::filesystem::rename(temp,target,ec);
    if(ec) std::filesystem::remove(temp,ec);
}
SavePreview read_save_preview(const std::filesystem::path &memory_stick) {
    std::error_code ec;
    const bool exists=std::filesystem::exists(save_file(memory_stick),ec);
    if(ec) return {};
    if(!exists) { SavePreview empty; empty.status=PreviewStatus::Missing; return empty; }
    const auto stamp=save_stamp(memory_stick,ec);
    if(ec) return {};
    SavePreview pending; pending.status=PreviewStatus::Pending;
    const auto path=preview_file(memory_stick);
    const auto size=std::filesystem::file_size(path,ec);
    if(ec) return pending;
    if(size>4096) return {};
    std::ifstream in(path,std::ios::binary);
    std::string line;
    if(!summary_line(in,line) || line!=kPreviewMagic) return {};
    // A save changed outside the game (imported, restored) has no summary yet.
    if(!summary_line(in,line) || line!="stamp "+stamp) return pending;
    return read_slots(in);
}
SavePreview read_preview_file(const std::filesystem::path &file) {
    std::error_code ec;
    const auto size=std::filesystem::file_size(file,ec);
    if(ec || size>4096) return {};
    std::ifstream in(file,std::ios::binary);
    std::string line;
    if(!summary_line(in,line) || line!=kPreviewMagic) return {};
    // A backup's copy: its stamp names the backed-up save, so it is not checked.
    if(!summary_line(in,line) || line.rfind("stamp ",0)!=0) return {};
    return read_slots(in);
}
SavePreview read_slots(std::istream &in) {
    std::string line;
    SavePreview result;
    result.status=PreviewStatus::Ready;
    for(std::size_t i=0;i<result.slots.size();++i) {
        if(!summary_line(in,line)) return {};
        std::istringstream row(line);
        std::string tag;
        std::size_t index=0;
        int occupied=-1;
        std::uint32_t rank=0, zenny=0, seconds=0;
        if(!(row>>tag>>index>>occupied>>rank>>zenny>>seconds) || tag!="slot" || index!=i ||
           occupied<0 || occupied>1 || rank>9) return {};
        if(!occupied) continue;
        std::string name;
        std::getline(row,name);
        if(!name.empty() && name.front()==' ') name.erase(0,1);
        if(name.empty() || rank<1) return {};
        auto &slot=result.slots[i];
        slot.occupied=true;
        slot.name=name;
        slot.hunter_rank=rank;
        slot.zenny=zenny;
        slot.played_seconds=seconds;
    }
    // Optional extension: old summaries and backup cards remain readable.
    while(summary_line(in,line)) {
        std::istringstream row(line); std::string tag,key; unsigned index=3;
        if(!(row>>tag>>index) || index>=3) continue;
        if(tag=="appearance" && row>>key && key.size()==64 &&
           key.find_first_not_of("0123456789abcdef")==std::string::npos)
            result.slots[index].appearance_key=key;
        else if(tag=="details") {
            std::uint32_t pokke=0,guild=0;
            if(row>>pokke>>guild) {
                result.slots[index].pokke_points=pokke; result.slots[index].guild_points=guild;
                result.slots[index].details_available=true;
            }
        } else if(tag=="gear") {
            unsigned e=6,present=0,type=0,id=0;
            if(row>>e>>present>>type>>id && e<6 && present<2 && type<=6 && id<=4095)
                result.slots[index].equipment[e]={present!=0,type,id};
        }
    }
    return result;
}
std::string format_play_time(std::uint32_t seconds) {
    char text[40];
    std::snprintf(text,sizeof(text),"%u:%02u:%02u",seconds/3600,(seconds/60)%60,seconds%60);
    return text;
}
}
