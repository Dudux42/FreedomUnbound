#include "equipment_preview.hpp"
#include "fu_text_table.hpp"
#include "equipment_style.hpp"
#include "psprecomp/corpus_abi.hpp"
#include <map>
#include "install/user_data.hpp"
#include <fstream>
#include <iterator>
#include <cstring>
#include <unordered_set>
#include <chrono>
#include <cstdio>
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "gpu/texture_decode.hpp"
#include "gpu/ge_state.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#endif
namespace portablekit::fu::equipment {
namespace {
auto folder() { return install::user_data_directory()/"save-preview"; }
}
void catalogue_seen(std::span<const std::uint8_t> bytes,const psprecomp::AotFastView &memory) {
    static bool saved=false;
    if(saved || bytes.size()>4*1024*1024 || !text::read_table(bytes)) return;
    std::error_code ec; std::filesystem::create_directories(folder(),ec);
    if(ec) return;
    std::ofstream out(folder()/"equipment-names.bin",std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
    out.flush();
    saved=bool(out);
    if(saved) {
        struct Reader {
            const psprecomp::AotFastView &m;
            auto load8(std::uint32_t a) const {return m.aot_load8(a);}
            auto load16(std::uint32_t a) const {return m.aot_load16(a);}
            auto load32(std::uint32_t a) const {return m.aot_load32(a);}
        } reader{memory};
        const auto table=text::read_table(bytes);
        constexpr unsigned sections[]={15,7,9,11,13,3,5};
        std::ofstream styles(folder()/"equipment-styles.txt",std::ios::trunc);
        styles<<"FUEQUIPMENT 1\n";
        for(unsigned type=0;type<7;++type) if(sections[type]<table->sections.size())
            for(unsigned id=1;id<std::min<std::size_t>(1501,table->sections[sections[type]].size());++id)
                if(auto style=read_style(reader,EquippedItem{true,type,id}))
                    styles<<type<<' '<<id<<' '<<style->page<<' '<<style->x<<' '<<style->y<<' '
                        <<style->width<<' '<<style->height<<' '<<style->tint<<'\n';
        styles.flush();saved=bool(styles);
    }
    if(saved) std::printf("[equipment-preview] cached equipment names\n");
}
std::string name(const EquippedItem &item) {
    if(!item.occupied) return "—";
    static std::filesystem::path path;
    static std::filesystem::file_time_type stamp{};
    static std::optional<text::StringTable> table;
    static auto next_check=std::chrono::steady_clock::time_point{};
    const auto current=folder()/"equipment-names.bin";
    const auto now=std::chrono::steady_clock::now();
    if(current!=path || now>=next_check) {
        next_check=now+std::chrono::seconds(1);
        if(current!=path) {table.reset();stamp={};path=current;}
        std::error_code ec; const auto modified=std::filesystem::last_write_time(current,ec);
        if(!ec && modified!=stamp) {
            stamp=modified;
            const auto size=std::filesystem::file_size(current,ec);
            if(!ec && size<=4*1024*1024) {
                std::ifstream in(current,std::ios::binary);
                std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in),{});
                table=text::read_table(bytes);
            }
        }
    }
    constexpr unsigned sections[]={15,7,9,11,13,3,5};
    if(item.type>=std::size(sections)) return "—";
    const auto section=sections[item.type];
    if(table && section<table->sections.size() && item.id<table->sections[section].size())
        if(const auto &value=table->sections[section][item.id]) return *value;
    return "#"+std::to_string(item.id);
}
#if defined(PORTABLEKIT_HAS_RENDERER)
namespace {
// FNV of the original decoded 256x256 common-menu icon sheet. This identifies
// the asset, not its guest address; replacement packs cannot change the source.
constexpr std::uint64_t kIconsHash[]={0x2063a90313a88502ull,0xcd7ac173745ac08eull};
std::array<bool,2> icons_saved{};
}
bool needs_icons() {
    static bool checked=false;
    if(!checked) {
        checked=true;
        for(unsigned page=0;page<2;++page) {
            std::error_code ec; const unsigned dimension=page?128:256;
            icons_saved[page]=std::filesystem::file_size(folder()/(page?"equipment-icons-extra.rgba":"equipment-icons.rgba"),ec)==dimension*dimension*4 && !ec;
        }
    }
    return !icons_saved[0] || !icons_saved[1];
}
void observe(const gpu::DrawCall &call,const psprecomp::GuestMemory &memory) {
    if(!call.through || !call.texture.enabled || call.texture.width!=call.texture.height) return;
    const unsigned dimension=call.texture.width;
    if(dimension!=256 && dimension!=128) return;
    const unsigned page=dimension==128?1:0;
    if(icons_saved[page]) return;
    static std::unordered_set<std::uint64_t> checked;
    const auto key=gpu::texture_key(memory,call.texture);
    if(checked.contains(key)) return;
    if(checked.size()>=8192) checked.clear();
    checked.insert(key);
    std::vector<std::uint32_t> decoded;
    if(!gpu::decode_texture(memory,call.texture,decoded)) return;
    std::uint64_t hash=14695981039346656037ull;
    for(auto value:decoded) for(unsigned n=0;n<4;++n) {hash^=(value>>(n*8))&255;hash*=1099511628211ull;}
    if(hash!=kIconsHash[page]) return;
    std::error_code ec;std::filesystem::create_directories(folder(),ec);if(ec)return;
    std::ofstream out(folder()/(page?"equipment-icons-extra.rgba":"equipment-icons.rgba"),std::ios::binary|std::ios::trunc);
    out.write(reinterpret_cast<const char *>(decoded.data()),decoded.size()*4);out.flush();icons_saved[page]=bool(out);
    if(icons_saved[page]) std::printf("[equipment-preview] cached original equipment icons page %u\n",page);
}
void draw_icon(const EquippedItem &item,float x,float y,float size) {
    if(!item.occupied || size<=0) return;
    static std::map<std::pair<unsigned,unsigned>,IconStyle> styles;
    static double style_retry=0;
    if(ImGui::GetTime()>=style_retry) {
        style_retry=ImGui::GetTime()+1;
        std::ifstream in(folder()/"equipment-styles.txt",std::ios::ate);
        if(in && in.tellg()<=512*1024) {
            in.seekg(0);std::string magic;unsigned version=0;
            if(in>>magic>>version && magic=="FUEQUIPMENT" && version==1) {
                std::map<std::pair<unsigned,unsigned>,IconStyle> fresh;
                unsigned type,id;IconStyle style;
                while(in>>type>>id>>style.page>>style.x>>style.y>>style.width>>style.height>>style.tint) {
                    const unsigned dimension=style.page?128:256;
                    if(type<7 && id<1501 && style.page<2 && style.width>0 && style.height>0 &&
                       style.width<=64 && style.height<=64 && style.x<=dimension-style.width && style.y<=dimension-style.height)
                        fresh[{type,id}]=style;
                }
                styles=std::move(fresh);
            }
        }
    }
    const auto found=styles.find({item.type,item.id});
    if(found==styles.end()) return;
    const auto &style=found->second;
    static std::array<ImTextureData *,2> textures{};
    static std::array<double,2> retry{};
    auto *&texture=textures[style.page];
    const unsigned dimension=style.page?128:256;
    if(!texture && ImGui::GetTime()>=retry[style.page]) {
        retry[style.page]=ImGui::GetTime()+1;
        std::ifstream in(folder()/(style.page?"equipment-icons-extra.rgba":"equipment-icons.rgba"),std::ios::binary|std::ios::ate);
        if(in && in.tellg()==dimension*dimension*4) {
            std::vector<std::uint32_t> pixels(dimension*dimension);in.seekg(0);
            if(in.read(reinterpret_cast<char *>(pixels.data()),pixels.size()*4)) {
                texture=new ImTextureData;texture->Create(ImTextureFormat_RGBA32,dimension,dimension);
                std::memcpy(texture->GetPixels(),pixels.data(),pixels.size()*4);ImGui::RegisterUserTexture(texture);
            }
        }
    }
    if(!texture || texture->GetTexID()==ImTextureID_Invalid) return;
    const float scale=size/std::max(style.width,style.height);
    ImGui::GetBackgroundDrawList()->AddImage(texture->GetTexRef(),{x,y},{x+style.width*scale,y+style.height*scale},
        {style.x/float(dimension),style.y/float(dimension)},
        {(style.x+style.width)/float(dimension),(style.y+style.height)/float(dimension)},style.tint);
}
#else
void observe(const gpu::DrawCall &,const psprecomp::GuestMemory &) {}
bool needs_icons() {return false;}
void draw_icon(const EquippedItem &,float,float,float) {}
#endif
}
