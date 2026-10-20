#include "equipment_wishlist.hpp"
#include "fu_text_table.hpp"
#include "install/user_data.hpp"
#include "psprecomp/runtime.hpp"
#include "wishlist_feedback.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <sstream>
#include <utility>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
namespace portablekit::fu::wishlist {
namespace {
std::mutex mutex;
bool enabled{};
bool was_town{};
unsigned slot{},held=3,requested{};
Key requested_key;
List list;
View current;
std::optional<PinFeedback> pin_feedback;
bool crafted_notice_pending{};
std::filesystem::path storage;
std::string hunter_key;
std::map<unsigned,std::string> names;
std::filesystem::path item_table_path;
std::filesystem::file_time_type item_table_stamp{};
std::optional<text::StringTable> item_table;
std::chrono::steady_clock::time_point item_table_check{};
unsigned menu{};
std::uint64_t menu_time{},frame_time{},marker_time{};
std::uint64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool valid(unsigned p,unsigned length,unsigned alignment=4) {
    return p>=0x08800000 && p<=0x0A000000-length && !(p%alignment);
}
std::string read_text(const psprecomp::AotFastView &m,unsigned p) {
    std::string result;if(!valid(p,257,1))return result;
    for(unsigned i=0;i<256;i++) {
        const auto b=m.aot_load8(p+i);if(!b)return result;
        if(b<32 || b==127)return {};
        result+=char(b);
    }
    return {};
}
bool lobby(const psprecomp::AotFastView &m) {
    return m.aot_load32(0x09AA5770)==0x27BDFDC0 && m.aot_load32(0x09AA49F8)==0x27BDFFF0;
}
void save() {
    if(storage.empty() || hunter_key.empty())return;
    std::error_code ec;std::filesystem::create_directories(storage,ec);if(ec)return;
    auto destination=storage/(hunter_key+".txt"),temporary=destination;temporary+=".tmp";
    {std::ofstream out(temporary,std::ios::binary|std::ios::trunc);list.write(out);out.flush();if(!out)return;}
#ifdef _WIN32
    if(!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
        std::fprintf(stderr,"[wishlist] could not persist pins (Windows error %lu); previous file retained\n",GetLastError());
#else
    std::filesystem::rename(temporary,destination,ec);
    if(ec)std::fprintf(stderr,"[wishlist] could not persist pins; previous file retained\n");
#endif
}
void identify(const psprecomp::AotFastView &m,unsigned state) {
    // The selected slot plus immutable UTF-16 hunter name. Equipment and other
    // mutable appearance fields are deliberately excluded from this identity.
    std::uint64_t hash=14695981039346656037ull;bool nonzero=false;
    for(unsigned i=0;i<16;i++){const auto b=m.aot_load8(state+0x4A0+i);nonzero|=b!=0;hash=(hash^b)*1099511628211ull;}
    if(!nonzero)return;
    hash=(hash^slot)*1099511628211ull;
    char key[24];std::snprintf(key,sizeof(key),"%016llx",static_cast<unsigned long long>(hash));
    if(hunter_key==key)return;
    list={};current.notices.clear();current.wishlist.clear();pin_feedback.reset();crafted_notice_pending=false;hunter_key=key;current.hunter=key;was_town=false;
    const auto path=storage/(hunter_key+".txt");std::error_code ec;
    const auto length=std::filesystem::file_size(path,ec);
    if(!ec && length<4*1024*1024) {
        std::ifstream in(path,std::ios::binary);
        if(!list.read(in)) {
            auto backup=path;backup+=".invalid";
            std::filesystem::copy_file(path,backup,std::filesystem::copy_options::skip_existing,ec);
            std::fprintf(stderr,"[wishlist] invalid pin file ignored\n");
        }
    }
}
std::optional<Recipe> selected(const psprecomp::AotFastView &m) {
    if(!valid(menu,112) || now_ns()-menu_time>200000000)return {};
    if(m.aot_load8(menu+32)!=2)return {}; // Native smithy, excluding the equipment shop.
    const auto count=m.aot_load32(menu+24),index=m.aot_load32(menu+64),rows=m.aot_load32(menu+16);
    if(index>8191 || (count && index>=count) || !valid(rows,(index+1)*16))return {};
    const auto row=rows+index*16;
    const unsigned type=m.aot_load8(row+10),equipment=m.aot_load16(row+12),id=m.aot_load16(row+14);
    const auto operation=m.aot_load8(menu+37);
    if(type>6 || !equipment || equipment>4095 || id>8191 || operation>1)return {};
    const unsigned category=type>=5 ? 1 : 0;
    // Creation uses indexed 26-byte records. Weapon upgrades use the target
    // equipment's 28-byte material record, exactly as the native smithy draws it.
    const unsigned recipe=operation ? (type==5 ? 0x0894FAE4u : 0x089578ACu)+equipment*28 : (category==0 ? 0x0893B938u : 0x0894944Cu)+id*26;
    if(!valid(recipe,28,2))return {};
    if(!operation && (m.aot_load8(recipe)!=type || m.aot_load16(recipe+2)!=equipment))return {};
    Recipe result{{operation,category,operation ? equipment : id,type,equipment},read_text(m,m.aot_load32(row+4)),{}};
    if(result.name.empty())return {};
    for(unsigned i=0;i<4;i++) {
        const unsigned offset=(operation ? 0u : 4u)+i*4;
        const auto item=m.aot_load16(recipe+offset),quantity=m.aot_load16(recipe+offset+2);
        if(!item && !quantity)continue;
        if(!item || item>=item_limit || !quantity || quantity>999)return {};
        result.materials.push_back({item,quantity});
    }
    if(result.materials.empty())return {};
    return result;
}
Inventory inventory(const psprecomp::AotFastView &m,unsigned state) {
    Inventory result{};
    for(unsigned group=0;group<2;group++) {
        const unsigned base=state+(group==0 ? 0x3428u : 0x43C8u),count=group==0 ? 1000 : 24;
        for(unsigned i=0;i<count;i++) {
            const auto id=m.aot_load16(base+i*4),quantity=m.aot_load16(base+i*4+2);
            if(id && id<item_limit && quantity && quantity<=99)result[id]+=quantity;
        }
    }
    return result;
}
}
void set_enabled(bool on,const std::filesystem::path &folder) {
    std::lock_guard lock(mutex);enabled=on;storage=folder;list={};current={};hunter_key.clear();
    menu=0;menu_time=frame_time=marker_time=0;held=3;requested=0;was_town=false;pin_feedback.reset();crafted_notice_pending=false;
}
void select_slot(unsigned value) {std::lock_guard lock(mutex);if(value<3)slot=value;}
void item_name(unsigned id,const std::string &value) {std::lock_guard lock(mutex);if(id && id<item_limit)names[id]=value;}
std::string item_name(unsigned id) {
    std::lock_guard lock(mutex);
    if(const auto found=names.find(id);found!=names.end())return found->second;
    const auto path=install::user_data_directory()/"save-preview"/"equipment-names.bin";
    const auto now=std::chrono::steady_clock::now();
    if(path!=item_table_path || now>=item_table_check) {
        item_table_check=now+std::chrono::seconds(1);
        if(path!=item_table_path){item_table.reset();item_table_stamp={};item_table_path=path;}
        std::error_code ec;const auto stamp=std::filesystem::last_write_time(path,ec);
        if(!ec && stamp!=item_table_stamp) {
            item_table_stamp=stamp;const auto size=std::filesystem::file_size(path,ec);
            if(!ec && size<=4*1024*1024) {
                std::ifstream in(path,std::ios::binary);std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(in),{});
                item_table=text::read_table(bytes);
            }
        }
    }
    if(item_table && item_table->sections.size()>1 && id<item_table->sections[1].size())
        if(const auto &name=item_table->sections[1][id];name)return *name;
    return "#"+std::to_string(id);
}
bool pinned(const Key &key) {std::lock_guard lock(mutex);return enabled && list.contains(key);}
bool toggle_pin(Recipe recipe) {
    std::lock_guard lock(mutex);if(!enabled || hunter_key.empty() || recipe.name.empty())return false;
    const auto key=recipe.key;const auto name=recipe.name;const bool was_pinned=list.contains(key);
    const bool changed=was_pinned?list.unpin(key):list.pin(std::move(recipe));
    if(!changed)return false;
    pin_feedback=PinFeedback{!was_pinned,name};save();return true;
}
unsigned triggers(bool left,bool right,bool available) {
    std::lock_guard lock(mutex);
    if(!available){held=3;requested=0;return 0;}
    const unsigned buttons=(left?1u:0u)|(right?2u:0u),pressed=buttons&~std::exchange(held,buttons);
    if(enabled && pressed && std::getenv("MHFU_WISHLIST_TRACE")) {
        std::printf("[wishlist-trigger] pressed=%u smithy=%u selected=%u age-ms=%llu\n",pressed,current.smithy,current.selected.has_value(),static_cast<unsigned long long>((now_ns()-frame_time)/1000000));std::fflush(stdout);
    }
    if(!enabled || !current.smithy || !current.selected || now_ns()-frame_time>200000000)return 0;
    if(pressed){requested=pressed;requested_key=current.selected->key;}
    return 3;
}
void capture_menu(const psprecomp::AllegrexContext &c,const psprecomp::AotFastView &m) {
    std::lock_guard lock(mutex);
    if(enabled && lobby(m) && valid(c.gpr[4],112)) {
        menu=c.gpr[4];menu_time=now_ns();
        static const bool trace=std::getenv("MHFU_WISHLIST_TRACE")!=nullptr;
        if(trace) {
            const auto index=m.aot_load32(menu+64),rows=m.aot_load32(menu+16);
            if(index<8192 && valid(rows,(index+1)*16)) {
                const auto row=rows+index*16;
                static std::string previous;
                const auto key=std::to_string(menu)+":"+std::to_string(index)+":"+std::to_string(m.aot_load8(menu+33))+":"+std::to_string(m.aot_load8(menu+35))+":"+std::to_string(m.aot_load8(menu+37));
                if(previous!=key) {
                    std::printf("[wishlist-menu] menu=%08X phase=%u kind=%u page=%u category=%u operation=%u count=%u index=%u row=%08X type=%u equipment=%u recipe=%u name=%s\n",
                        menu,m.aot_load8(menu+33),m.aot_load8(menu+34),m.aot_load8(menu+35),m.aot_load8(menu+36),m.aot_load8(menu+37),m.aot_load32(menu+24),index,row,
                        m.aot_load8(row+10),m.aot_load16(row+12),m.aot_load16(row+14),read_text(m,m.aot_load32(row+4)).c_str());
                    std::fflush(stdout);previous=key;
                }
            }
        }
    }
}
void frame(const psprecomp::AotFastView &m) {
    std::lock_guard lock(mutex);if(!enabled)return;
    const auto state=m.aot_load32(0x089CC558);
    const bool town=lobby(m),play=m.aot_load32(0x09A6B154)==0x106005F7;
    current.quest=false;current.smithy=false;current.finish_notices=false;current.village=false;current.selected.reset();
    if(!(town || play) || !valid(state,0x69700)) {
        current.finish_notices=!hunter_key.empty();was_town=false;return;
    }
    identify(m,state);if(hunter_key.empty())return;
    current.village=town;
    const auto phase=m.aot_load8(state+46);
    const auto quest=m.aot_load32(0x08A62D9C);
    const bool returning=phase==3 && valid(quest,4) && m.aot_load8(quest)==1 && m.aot_load8(quest+1)<=2;
    current.quest=play && (phase==2 || returning);
    current.finish_notices=town || (play && (phase==3 || phase==4));
    list.observe(inventory(m,state),current.quest);
    current.wishlist.clear();
    for(const auto &recipe:list.recipes())current.wishlist.push_back({recipe.key,recipe.name,list.checklist(recipe.key)});
    if(town && !was_town)list.announce_ready();
    was_town=town;
    auto events=list.take_notices();
    if(current.quest || town)current.notices.insert(current.notices.end(),events.begin(),events.end());
    else if(!current.finish_notices)current.notices.clear();
    if(town)current.selected=selected(m);
    current.smithy=current.selected.has_value();
    if(requested) {
        if(std::getenv("MHFU_WISHLIST_TRACE")) {
            std::printf("[wishlist-request] buttons=%u selected=%u matching=%u\n",requested,current.selected.has_value(),current.selected && current.selected->key==requested_key);std::fflush(stdout);
        }
        if(current.selected && current.selected->key==requested_key) {
            const bool changed=(requested&1) ? list.unpin(requested_key) : list.pin(*current.selected);
            if(changed){current.markers.clear();pin_feedback=PinFeedback{!(requested&1),current.selected->name};save();std::printf("[wishlist] %s %s\n",(requested&1)?"unpinned":"pinned",current.selected->name.c_str());std::fflush(stdout);}
        }
        requested=0;
    }
    current.pinned=current.selected && list.contains(current.selected->key);
    frame_time=now_ns();
    if(!current.smithy || frame_time-marker_time>200000000)current.markers.clear();
}
void capture_material(const psprecomp::AllegrexContext &c,const psprecomp::AotFastView &m,bool upgrade) {
    std::lock_guard lock(mutex);
    if(!enabled || !current.smithy || !current.pinned || !current.selected || !lobby(m))return;
    const unsigned source=c.gpr[upgrade?19:18];if(!valid(source,8,2))return;
    const auto item=m.aot_load16(source+(upgrade?0:4));
    const int x=static_cast<std::int16_t>(c.gpr[16]),y=static_cast<std::int16_t>(c.gpr[17]);
    if(x<10 || x>=480 || y<0 || y>=260)return;
    if(std::none_of(current.selected->materials.begin(),current.selected->materials.end(),[&](auto material){return material.item==item;}))return;
    const Marker marker{float(x-7),float(y+6)};
    for(auto previous:current.markers)if(previous.x==marker.x && previous.y==marker.y)return;
    if(current.markers.size()<32)current.markers.push_back(marker);marker_time=now_ns();
}
void capture_craft(const psprecomp::AllegrexContext &c,const psprecomp::AotFastView &m) {
    std::lock_guard lock(mutex);
    if(!enabled || hunter_key.empty() || !lobby(m))return;
    // Native smithy commit return: s4=menu, s3=type, s2=target, s1=box slot.
    // Failed insertion returns FFFF. Validate the written slot before unpinning.
    const auto state=m.aot_load32(0x089CC558),box_slot=c.gpr[17];
    if(!valid(state,0x69700) || box_slot>=1000 || c.gpr[20]!=menu)return;
    menu_time=now_ns();
    const auto recipe=selected(m);
    if(!recipe || recipe->key.type!=(c.gpr[19]&255) || recipe->key.equipment!=(c.gpr[18]&65535))return;
    const unsigned equipment=state+0x4A0+168+box_slot*12;
    if(m.aot_load8(equipment)!=1 || m.aot_load8(equipment+1)!=recipe->key.type ||
       m.aot_load16(equipment+2)!=recipe->key.equipment)return;
    if(list.unpin(recipe->key)) {
        current.pinned=false;current.markers.clear();
        std::erase_if(current.notices,[&](const Notice &n){return n.complete && n.equipment==recipe->name;});
        crafted_notice_pending=true;save();
        std::printf("[wishlist] crafted, unpinned %s\n",recipe->name.c_str());std::fflush(stdout);
    }
}
void capture_text(const psprecomp::AllegrexContext &c,const psprecomp::AotFastView &m,unsigned point) {
    std::lock_guard lock(mutex);
    if(!enabled || !current.smithy || !current.selected || now_ns()-frame_time>200000000 || !lobby(m))return;
    const int x=static_cast<std::int16_t>(c.gpr[5]),y=static_cast<std::int16_t>(c.gpr[6]);
    if(x<10 || x>=480 || y<0 || y>=260)return;
    const bool shadow=point==0x0889198C || point==0x08891A28;
    const auto format=read_text(m,c.gpr[shadow?9:point==0x088915B0?8:7]);
    const auto argument=read_text(m,c.gpr[point==0x08891474?8:9]);
    bool wanted=x>=240 && y<190 && (list.contains_name(format) || list.contains_name(argument));
    for(auto material:current.selected->materials) {
        if(!names.contains(material.item))continue;
        const auto &name=names.at(material.item);
        const bool visible_name=format==name || argument==name;
        if(!visible_name)continue;
        for(auto requirement:list.checklist(current.selected->key))wanted|=requirement.item==material.item;
    }
    if(!wanted)return;
    const Marker marker{float(x-8),float(y+6)};
    for(auto previous:current.markers)if(previous.x==marker.x && previous.y==marker.y)return;
    if(current.markers.size()<32)current.markers.push_back(marker);
    marker_time=now_ns();
}
View view(bool consume) {
    std::lock_guard lock(mutex);auto result=enabled ? current : View{};
    if(now_ns()-frame_time>500000000){result.quest=false;result.smithy=false;result.finish_notices=false;result.village=false;result.markers.clear();result.notices.clear();}
    if(now_ns()-marker_time>100000000)result.markers.clear();
    if(consume){current.notices.clear();current.markers.clear();}
    return result;
}
std::optional<PinFeedback> take_pin_feedback() {std::lock_guard lock(mutex);return std::exchange(pin_feedback,{});}
bool has_pin_feedback() {std::lock_guard lock(mutex);return enabled && pin_feedback.has_value();}
bool take_crafted_notice() {std::lock_guard lock(mutex);return enabled && std::exchange(crafted_notice_pending,false);}
bool has_crafted_notice() {std::lock_guard lock(mutex);return enabled && crafted_notice_pending;}
}
