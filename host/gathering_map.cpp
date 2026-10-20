#include "gathering_map.hpp"
#include "psprecomp/runtime.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
namespace portablekit::fu::gathering_map {
namespace {
std::mutex mutex;
bool enabled{};
View current;
std::atomic<unsigned> filter_mask{31};
bool valid(unsigned p,unsigned size,unsigned alignment=4) {
    return p>=0x08800000 && size<=0x01800000 && p<=0x0A000000-size && !(p%alignment);
}
float number(const psprecomp::AotFastView &m,unsigned p) {return std::bit_cast<float>(m.aot_load32(p));}
bool finite(float f) {return std::isfinite(f) && std::abs(f)<1.e8f;}
}
void set_enabled(bool on) {std::lock_guard lock(mutex);enabled=on;current={};}
unsigned filters() {return filter_mask.load();}
void set_filters(unsigned mask) {filter_mask.store(mask & 31);}
View snapshot() {
    std::lock_guard lock(mutex);auto view=current;const auto mask=filters();
    std::erase_if(view.markers,[mask](const Marker &m){return !(mask & (1u << unsigned(m.kind)));});
    return view;
}
void capture(const psprecomp::AllegrexContext &c,const psprecomp::AotFastView &m,bool zoomed) {
    std::lock_guard lock(mutex);if(!enabled)return;
    current={};
    const auto state=m.aot_load32(0x089CC558),quest=m.aot_load32(0x08A62D9C),hud=c.gpr[21];
    if(m.aot_load32(0x09A6F490)!=0x27BDFFF0 || !valid(state,0x6AF11) || !valid(quest,100) || !valid(hud,1400))return;
    const auto phase=m.aot_load8(state+46);
    if(phase!=2 && !(phase==3 && m.aot_load8(quest)==1 && m.aot_load8(quest+1)<=2))return;
    const auto hunter=m.aot_load32(hud+1328);
    if(!valid(hunter,668) || hunter!=m.aot_load32(0x099998F4))return;
    const auto current_zone=m.aot_load16(hunter+0x29A);
    const auto area=m.aot_load8(state+0x6AF10);
    const auto mapping=0x09CE4808+unsigned(area)*8;
    const auto zones=m.aot_load32(mapping),ids=m.aot_load32(mapping+4);
    const auto offsets=m.aot_load32(quest+0x60),data=m.aot_load32(quest+0x50);
    const bool static_nodes=zones && zones<=256 && valid(ids,zones*2,2) && valid(offsets,zones*4) && valid(data,24);
    // Observe the native local-hunter arrow after its map projection. Derive
    // the affine origin from that same arrow, including map padding/panning.
    const float sx=zoomed ? 760.f*number(m,hud+1380) : c.fpr[23];
    const float sy=zoomed ? 760.f*number(m,hud+1384) : c.fpr[22];
    const float ox=c.fpr[12]-number(m,hunter+608)*sx;
    const float oy=c.fpr[13]-number(m,hunter+616)*sy;
    const float size=zoomed ? c.fpr[21]*(760.f/150.f) : 150.f*c.fpr[24];
    const float left=zoomed ? float(c.gpr[30]) : float(c.gpr[22]);
    if(zoomed && !valid(c.gpr[29]+110,2,2))return;
    const float top=zoomed ? float(static_cast<std::int16_t>(m.aot_load16(c.gpr[29]+110))) : float(c.gpr[23]);
    static const bool trace=std::getenv("MHFU_GATHERING_MAP_TRACE")!=nullptr;
    static std::chrono::steady_clock::time_point last_trace[2];
    const auto now=std::chrono::steady_clock::now();
    const bool report=trace && now-last_trace[zoomed]>std::chrono::seconds(2);
    if(report) {
        last_trace[zoomed]=now;
        std::printf("[gathering-map] zoom=%u zones=%u arrow=%.2f,%.2f scale=%.6f,%.6f rect=%.1f,%.1f,%.1f origin=%.2f,%.2f\n",zoomed,zones,c.fpr[12],c.fpr[13],sx,sy,left,top,size,ox,oy);
        std::fflush(stdout);
    }
    for(float f:{sx,sy,ox,oy,size,left,top})if(!finite(f))return;
    if(sx==0 || sy==0 || size<8 || size>544 || left<0 || top<0 || left+size>481 || top+size>273)return;
    View result{left,top,left+size,top+size};
    for(unsigned zone=0;static_nodes && zone<zones;zone++) {
        const auto id=m.aot_load16(ids+zone*2);
        if(id>1023 || id!=current_zone)continue;
        const auto translation=0x09CDF040+unsigned(id)*32;
        const float tx=number(m,translation),tz=number(m,translation+4);
        if(!finite(tx) || !finite(tz))continue;
        const auto offset=m.aot_load32(offsets+zone*4);
        if(!offset || offset>0x01800000 || !valid(data+offset,24))continue;
        auto node=data+offset;
        for(unsigned i=0;i<256 && valid(node,24);i++,node+=24) {
            const float x=number(m,node),z=number(m,node+8);
            if(x==-1.f)break;
            const float radius=number(m,node+12);
            if(!finite(x) || !finite(z) || !finite(radius) || radius<0 || radius>10000)break;
            const auto type=m.aot_load16(node+20);
            if(!m.aot_load16(node+18) || m.aot_load16(node+16)==0xFFFF || type>=0x8000)continue;
            const float px=ox+(x+tx)*sx,py=oy+(z+tz)*sy;
            if(!finite(px) || !finite(py) || px<result.left || px>result.right || py<result.top || py>result.bottom)continue;
            // Static map type 2 is fishing; temporary interaction type 2 below is carving.
            const auto kind=type==3 ? Kind::Mine : type==4 ? Kind::Bugs : type==2 ? Kind::Fishing : Kind::Gather;
            // Duplicate native entries at the same spot need only one symbol.
            if(std::any_of(result.markers.begin(),result.markers.end(),[&](const Marker &a){
                return a.kind==kind && std::abs(a.x-px)<1 && std::abs(a.y-py)<1;
            }))continue;
            result.markers.push_back({px,py,kind});
            if(result.markers.size()>=4096)break;
        }
        if(result.markers.size()>=4096)break;
    }
    // Native temporary interaction registry: 20 records at manager+4, stride
    // 32 (0886CC24 insertion, 0886CEA8 area query, 0886CE10 removal).
    // Type 2 is carving; the native insertion path applies Carving skill uses.
    // Read this registry rather than inferring carve eligibility from HP.
    const auto temporary=m.aot_load32(0x09A4F1A0);
    if(current_zone<=1023 && valid(temporary,644) && m.aot_load32(0x0886CC24)==0x27BDFFE0) {
        const auto translation=0x09CDF040+unsigned(current_zone)*32;
        const float tx=number(m,translation),tz=number(m,translation+4);
        if(finite(tx) && finite(tz))for(unsigned i=0;i<20;++i) {
            const auto node=temporary+4+i*32;
            if(m.aot_load16(node+16)==0xFFFF || !m.aot_load16(node+18) ||
               m.aot_load16(node+20)!=2 || m.aot_load16(node+24)!=current_zone)continue;
            const float x=number(m,node),z=number(m,node+8),radius=number(m,node+12);
            if(!finite(x) || !finite(z) || !finite(radius) || radius<0 || radius>10000)continue;
            const float px=ox+(x+tx)*sx,py=oy+(z+tz)*sy;
            if(!finite(px) || !finite(py) || px<result.left || px>result.right || py<result.top || py>result.bottom)continue;
            if(std::any_of(result.markers.begin(),result.markers.end(),[&](const Marker &a){
                return a.kind==Kind::Carve && std::abs(a.x-px)<1 && std::abs(a.y-py)<1;
            }))continue;
            result.markers.push_back({px,py,Kind::Carve});
        }
    }
    result.timestamp=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    current=std::move(result);
    if(report){std::printf("[gathering-map] visible=%zu\n",current.markers.size());std::fflush(stdout);}
}
}
