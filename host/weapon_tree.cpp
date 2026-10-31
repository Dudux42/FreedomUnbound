#include "weapon_tree.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <chrono>
#include <map>
#include <mutex>
#include <queue>
#include <set>
namespace portablekit::fu::weapon_tree {
namespace {
std::mutex mutex;
bool enabled{};
unsigned selected_type{},selected_id{};
std::uint64_t seen{};
std::optional<Chart> chart;
auto now() {return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
bool valid(unsigned p,unsigned length) {return p>=0x08800000 && p<=0x0A000000-length && !(p&3);}
bool town(const psprecomp::AotFastView &m) {return m.aot_load32(0x09AA5770)==0x27BDFDC0 && m.aot_load32(0x09AA49F8)==0x27BDFFF0;}
void select(const psprecomp::AotFastView &m,unsigned type,unsigned id) {
    if((type!=5 && type!=6) || !id) {selected_id=selected_type=0;chart.reset();return;}
    if(type!=selected_type || id!=selected_id) {
        selected_type=type;selected_id=id;chart=build(m,type,id);
    }
    seen=now();
}
}
std::optional<Weapon> read_weapon(const psprecomp::AotFastView &m,unsigned type,unsigned id) {
    if((type!=5 && type!=6) || !id || id>=(type==5?1150u:354u))return {};
    const auto record=(type==5 ? 0x0895A0ECu+24*id : 0x08960CBCu+28*id);
    const auto model=m.aot_load16(record);
    if(model>=2048)return {};
    Weapon result;result.type=type;result.id=id;result.category=m.aot_load8(0x089785A8+model);result.rarity=m.aot_load8(record+2);
    if(result.category>10 || result.rarity>9)return {};
    const auto scale=m.aot_load32(0x0893ACDC+result.category*4);
    const auto raw=m.aot_load16(record+(type==5?10:8));
    if(!raw || raw>2000 || !scale || scale>1000)return {};
    result.power=raw*scale/100;
    if(type==5) {
        result.weapon_variable=m.aot_load8(record+19);
        for(unsigned i=0;i<2;i++) {
            const auto effect=m.aot_load8(record+14+i*2);
            if(effect>(i?3u:5u))return {};
            result.elements[i]=effect && i==1?effect+5:effect;result.values[i]=m.aot_load8(record+15+i*2)*10;
        }
        // Native dual-attribute exceptions are separate from normal effect bytes.
        for(unsigned i=0;i<15;i++) {
            const auto dual=0x08979878u+i*10;
            if(m.aot_load16(dual)!=id)continue;
            for(unsigned j=0;j<2;j++) {
                const unsigned bits=m.aot_load16(dual+2+j*4);
                if(bits && bits<=128 && !(bits&(bits-1))) {
                    result.elements[j]=std::countr_zero(bits)+1;result.values[j]=m.aot_load16(dual+4+j*4)*10;
                }
            }
            break;
        }
        // Original sharpness resolver 088DABB0: class pointer, profile*14,
        // seven cumulative thresholds. Native maximum is 150 + tier*50.
        const auto profiles=m.aot_load32(0x0897935C+result.category*4);
        const auto profile=m.aot_load8(record+8),tier=m.aot_load8(record+9);
        if(tier>6 || !valid(profiles,14*(profile+1)))return {};
        unsigned previous=0;const unsigned cap=150+tier*50;
        for(unsigned i=0;i<7;i++) {
            const auto end=unsigned(m.aot_load16(profiles+profile*14+i*2));
            if(end<previous || end>1000)return {};
            result.sharpness[i]=std::min(end,cap)-std::min(previous,cap);previous=end;
        }
    } else if(result.category==10) {
        result.elements[0]=m.aot_load8(record+17);result.values[0]=m.aot_load8(record+18)*10;
        if(result.elements[0]>5)return {};
        result.charges=m.aot_load8(record+11);
        const auto pattern=m.aot_load8(record+16);
        if(result.charges<2 || result.charges>4 || pattern>=117)return {};
        for(unsigned i=0;i<4;i++){result.shots[i]=m.aot_load8(0x08978F58+pattern*4+i);if(result.shots[i]>=15)return {};}
    } else {
        result.recoil=m.aot_load8(record+11);result.reload=m.aot_load8(record+19);
        const auto ammo=m.aot_load8(record+16),variant=m.aot_load8(record+3);
        if(result.recoil>6 || result.reload>4 || ammo>=58 || variant>=4)return {};
        const auto mods=m.aot_load32(0x08979568+variant*4);
        if(!valid(mods,24))return {};
        result.deviation=std::bit_cast<float>(m.aot_load32(mods+16));
        if(!std::isfinite(result.deviation) || result.deviation<0 || result.deviation>100)return {};
        for(unsigned i=0;i<32;i++)result.ammo[i]=m.aot_load8(0x08978818+ammo*32+i);
    }
    result.ammo_mask=type==6?m.aot_load32(record+24):0;
    const auto recipe=(type==5?0x0894FAE4u:0x089578ACu)+id*28;
    for(unsigned i=0;i<5;i++) {
        const auto child=unsigned(m.aot_load16(recipe+16+i*2));
        if(child && child<1501 && child!=id && std::find(result.next.begin(),result.next.end(),child)==result.next.end())result.next.push_back(child);
    }
    return result;
}
Chart build(const psprecomp::AotFastView &m,unsigned type,unsigned id) {
    Chart out{type,id};
    std::map<unsigned,Weapon> all;std::map<unsigned,std::vector<unsigned>> adjacent;
    for(unsigned n=1;n<1501;n++)if(auto weapon=read_weapon(m,type,n))all.emplace(n,std::move(*weapon));
    if(!all.contains(id))return out;
    for(const auto &[parent,w]:all)for(auto child:w.next)if(all.contains(child)) {adjacent[parent].push_back(child);adjacent[child].push_back(parent);}
    std::set<unsigned> component;std::queue<unsigned> pending;pending.push(id);component.insert(id);
    while(!pending.empty()) {const auto n=pending.front();pending.pop();for(auto next:adjacent[n])if(component.insert(next).second)pending.push(next);}
    std::map<unsigned,unsigned> degree,columns,rows;
    for(auto n:component)for(auto child:all.at(n).next)if(component.contains(child))degree[child]++;
    for(auto n:component)if(!degree[n])pending.push(n);
    std::set<unsigned> placed;
    while(!pending.empty()) {
        const auto n=pending.front();pending.pop();placed.insert(n);
        for(auto child:all.at(n).next)if(component.contains(child)) {
            columns[child]=std::max(columns[child],columns[n]+1);
            if(--degree[child]==0)pending.push(child);
        }
    }
    // Invalid cycles are excluded rather than recursed forever.
    for(auto n:component)if(placed.contains(n)) {
        out.nodes.push_back({all.at(n),columns[n],rows[columns[n]]++});
        for(auto child:all.at(n).next)if(placed.contains(child))out.edges.push_back({n,child});
    }
    // Forge recipes are indexed separately from equipment IDs. Build the
    // lookup once per chart and use an upgrade recipe for every non-root node.
    std::map<unsigned,unsigned> forge_recipe;
    constexpr unsigned forge_base=0x0894944Cu,forge_stride=26;
    for(unsigned recipe=0;recipe<1504;recipe++) {
        const auto address=forge_base+recipe*forge_stride;
        const auto recipe_type=m.aot_load8(address);
        const auto equipment=m.aot_load16(address+2);
        if(recipe_type==type && equipment && equipment<1501)forge_recipe.try_emplace(equipment,recipe);
    }
    std::set<unsigned> has_parent;
    for(const auto &[parent,children]:adjacent)for(auto child:children)
        if(all.contains(parent) && std::find(all.at(parent).next.begin(),all.at(parent).next.end(),child)!=all.at(parent).next.end())has_parent.insert(child);
    for(auto &node:out.nodes) {
        auto &weapon=node.weapon;
        const bool can_upgrade=has_parent.contains(weapon.id);
        unsigned address=0,offset=0;
        if(can_upgrade) {weapon.operation=1;weapon.recipe=weapon.id;address=(type==5?0x0894FAE4u:0x089578ACu)+weapon.id*28;}
        else if(auto found=forge_recipe.find(weapon.id);found!=forge_recipe.end()) {
            weapon.operation=0;weapon.recipe=found->second;address=forge_base+weapon.recipe*forge_stride;offset=4;
        }
        if(!address)continue;
        for(unsigned i=0;i<4;i++) {
            const auto item=m.aot_load16(address+offset+i*4),quantity=m.aot_load16(address+offset+i*4+2);
            if(!item && !quantity)continue;
            if(!item || item>=2048 || !quantity || quantity>999) {weapon.material_count=0;break;}
            weapon.materials[weapon.material_count++]={item,quantity};
        }
        if(!weapon.material_count)weapon.operation=0;
    }
    return out;
}
void set_enabled(bool on) {std::lock_guard lock(mutex);enabled=on;chart.reset();selected_id=selected_type=0;seen=0;}
void capture_menu(const psprecomp::AllegrexContext &c,const psprecomp::AotFastView &m) {
    std::lock_guard lock(mutex);if(!enabled || !town(m))return;
    const auto menu=c.gpr[4];if(!valid(menu,112) || m.aot_load8(menu+32)!=2)return;
    const auto index=m.aot_load32(menu+64),rows=m.aot_load32(menu+16);
    // menu+24 counts only the visible page's rows; menu+64 indexes the whole list.
    // The native smithy reads rows+index*16 unchecked, so pages past the first must work.
    if(index>=8192 || !valid(rows,(index+1)*16))return;
    const auto row=rows+index*16,type=unsigned(m.aot_load8(row+10)),id=unsigned(m.aot_load16(row+12));
    select(m,type,id);
}
void capture_grid_text(const psprecomp::AllegrexContext &c,const psprecomp::AotFastView &m,unsigned point) {
    // This return address is the Improve Weapon equipment grid's Info prompt.
    // s1 retains its menu pointer. The native eligibility routines 09AA4828 /
    // 09AA4930 read the highlighted box slot from menu+64, in 12-byte records.
    if(point!=0x08891474 || c.gpr[31]!=0x09A82140)return;
    std::lock_guard lock(mutex);if(!enabled || !town(m))return;
    const auto menu=c.gpr[17],state=m.aot_load32(0x089CC558);
    if(!valid(menu,112) || m.aot_load8(menu+32)!=2 || m.aot_load8(menu+33)!=6 ||
       m.aot_load8(menu+37)!=1 || !valid(state,0x69700))return;
    const auto index=m.aot_load32(menu+64);
    if(index>=1000)return;
    const auto item=state+0x548+index*12;
    select(m,m.aot_load8(item+1),m.aot_load16(item+2));
}
void frame(const psprecomp::AotFastView &m) {
    std::lock_guard lock(mutex);
    if(!town(m) || now()-seen>300000000){chart.reset();selected_id=0;return;}
    if(!chart)return;
    const auto state=m.aot_load32(0x089CC558);if(!valid(state,0x69700))return;
    std::set<std::pair<unsigned,unsigned>> owned;
    for(unsigned slot=0;slot<1000;slot++) {
        const auto item=state+0x4A0+168+slot*12;
        if(m.aot_load8(item)!=1)continue;
        const auto type=m.aot_load8(item+1);
        const auto id=unsigned(m.aot_load16(item+2));
        owned.emplace(type,id);
    }
    for(auto &node:chart->nodes)node.weapon.owned=owned.contains({node.weapon.type,node.weapon.id});
}
std::optional<Chart> snapshot() {std::lock_guard lock(mutex);if(!enabled || now()-seen>300000000)return {};return chart;}
bool available() {std::lock_guard lock(mutex);return enabled && chart && !chart->nodes.empty() && now()-seen<=300000000;}
}
