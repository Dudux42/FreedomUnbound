#pragma once
#include "save_preview.hpp"
#include <optional>

namespace portablekit::fu::equipment {
struct IconStyle {
    unsigned page{}, x{}, y{}, width{}, height{};
    std::uint32_t tint{};
};
// Independently implements the supported USA equipment icon/rarity lookups.
// All sprite IDs, rectangles and palette values are read from the player's
// executable in memory; no game artwork or palette ships with this code.
// References: 0885C83C, 0885CD18, 0883029C, 08830354 and 0882B624.
template<class Memory>
std::optional<IconStyle> read_style(const Memory &m,const EquippedItem &item) {
    if(!item.occupied || !item.id || item.type>6 || item.id>=1501) return {};
    unsigned rarity=0, sprite=0;
    if(item.type<5) {
        const auto base=m.load32(0x08978594+item.type*4);
        if(base<0x08800000 || base+40*item.id+40>0x0a000000) return {};
        rarity=m.load8(base+40*item.id+5);
        sprite=m.load8(0x0893afcc+item.type);
    } else {
        const auto address=item.type==5?0x0895a0ec+24*item.id:0x08960cbc+28*item.id;
        rarity=m.load8(address+2);
        const auto model=m.load16(address);
        if(model>=2048) return {};
        const auto category=m.load8(0x089785a8+model);
        if(category>10) return {};
        sprite=m.load16(0x0893afa0+category*2);
    }
    if(rarity>9 || sprite>=512) return {};
    const auto color_index=m.load8(0x0893b02c+rarity);
    if(color_index>=16) return {};
    const auto color=m.load32(0x0893afec+color_index*4);
    // The original sprite path packs its BGR table into RGB555, then the GE
    // expands each channel to eight bits. Preserve that exact quantization.
    const auto channel=[](unsigned v) {v=(v&255)>>3;return v*255/31;};
    const auto tint=0xff000000u | channel(color>>16) | (channel(color>>8)<<8) | (channel(color)<<16);
    const auto rect=0x089392c8+sprite*12;
    IconStyle result{unsigned(m.load16(rect+8)&4095),m.load16(rect),m.load16(rect+2),m.load16(rect+4),m.load16(rect+6),tint};
    const auto dimension=result.page==0?256u:128u;
    if(result.page>1 || !result.width || !result.height || result.x+result.width>dimension || result.y+result.height>dimension) return {};
    return result;
}
}
