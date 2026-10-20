#include "direct_load.hpp"
#include "direct_load_policy.hpp"
#include "launcher_art.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/sha256.hpp"
#include "audio/audio_sink.hpp"
#include <chrono>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "gpu/vulkan_renderer.hpp"
#include "ui/host_text.hpp"
#include "ui/menu_request.hpp"
#include "imgui.h"
#endif
namespace portablekit::fu::direct_load {
namespace {
bool pending=false,active=false,verified=false;
unsigned target=3,flips=0;
HunterSummary expected;
std::unique_ptr<Navigation> navigation;
SelectorObserver selector;
std::uint32_t buttons=0;
std::string message;
std::chrono::steady_clock::time_point started{},message_until{};
void stop(const char *reason) {
    active=false; buttons=0;
    audio::AudioSink::instance().set_output_suppressed(false);
    message=reason;
#if defined(PORTABLEKIT_HAS_RENDERER)
    message=ui::host_text::get(reason);
#endif
    message_until=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    std::printf("[direct-load] slot=%u %s\n",target+1,reason); std::fflush(stdout);
}
}
bool enabled() { const auto *v=std::getenv("MHFU_DISABLE_DIRECT_LOAD"); return !v || v[0]!='1'; }
void request(unsigned slot, const HunterSummary &hunter) {
    if(!enabled() || slot>=3 || !same_hunter(hunter,hunter)) return;
    target=slot; expected=hunter; pending=true;
}
bool take_start() {
    if(!pending) return false;
    pending=false; active=true; verified=false; flips=0; buttons=0;
    audio::AudioSink::instance().set_output_suppressed(true);
    navigation=std::make_unique<Navigation>(target);
    selector={};
    started={}; message.clear();
#if defined(PORTABLEKIT_HAS_RENDERER)
    ui::set_menu_request_filter([] {
        if(!active) return false;
        stop("launcher.save.load_cancelled"); return true;
    });
#endif
    std::printf("[direct-load] requested slot=%u hunter=%s\n",target+1,expected.name.c_str()); std::fflush(stdout);
    return true;
}
void reset_request() {
    pending=false; active=false; buttons=0; navigation.reset(); message.clear();
    audio::AudioSink::instance().set_output_suppressed(false);
}
void filter_pad(gpu::PadState &pad) {
#if defined(PORTABLEKIT_HAS_RENDERER)
    if(!active) return;
    // A click which launched the game may still be held for its first flips.
    if(flips>15 && pad.buttons) { stop("launcher.save.load_cancelled"); pad={}; return; }
    pad={}; pad.buttons=buttons;
#endif
}
void save_seen(const SavePreview &preview) {
    if(!active) return;
    if(preview.status!=PreviewStatus::Ready || target>=3 || !same_hunter(preview.slots[target],expected)) {
        stop("launcher.save.load_changed"); return;
    }
    verified=true;
    std::printf("[direct-load] verified slot=%u hunter=%s\n",target+1,expected.name.c_str()); std::fflush(stdout);
}
bool visible() { return active || std::chrono::steady_clock::now()<message_until; }
void draw() {
#if defined(PORTABLEKIT_HAS_RENDERER)
    if(!visible()) return;
    auto *d=ImGui::GetForegroundDrawList();
    const auto size=ImGui::GetIO().DisplaySize;
    if(active) {
        const float logo_width=std::min(size.x*.72f,size.y*1.05f);
        draw_loading_art((size.x-logo_width)*.5f,size.y*.14f,logo_width,size.y*.40f);
        const float font_size=std::clamp(size.y*.038f,14.0f,32.0f);
        const auto text=ui::host_text::format("launcher.save.loading",{expected.name});
        const auto extent=ImGui::GetFont()->CalcTextSizeA(font_size,1e6f,0,text.c_str());
        const ImVec2 at{(size.x-extent.x)*.5f,size.y*.64f};
        d->AddRectFilled({at.x-font_size,at.y-font_size*.5f},{at.x+extent.x+font_size,at.y+font_size*1.6f},IM_COL32(10,20,26,205),font_size*.5f);
        d->AddText(ImGui::GetFont(),font_size,at,IM_COL32(218,245,200,255),text.c_str());
        const char *hint=ui::host_text::cstr("launcher.save.cancel_load");
        const auto h=ImGui::CalcTextSize(hint);
        d->AddText({(size.x-h.x)*.5f,size.y*.76f},IM_COL32(232,238,242,255),hint);
    } else if(!message.empty()) {
        d->AddRectFilled({0,0},{size.x,ImGui::GetFontSize()*2.5f},IM_COL32(10,20,26,240));
        d->AddText({ImGui::GetFontSize(),ImGui::GetFontSize()*.6f},IM_COL32(240,220,170,255),message.c_str());
    }
#endif
}
void frame(psprecomp::Runtime &runtime) {
    const auto &m=runtime.memory();
    if(active) {
        const auto now=std::chrono::steady_clock::now();
        if(started==std::chrono::steady_clock::time_point{}) started=now;
        ++flips;
        if(now-started>std::chrono::seconds(90)) { stop("launcher.save.load_timeout"); return; }
        MenuState observed;
        // USA overlay identities: the title/demo and Continue tasks share a
        // memory window. Never drive an arbitrary overlay occupying that slot.
        const bool boot=m.load32(0x09a5f390)==0x8c84001c && m.load32(0x09a5f3d0)==0x1000004f && m.load32(0x09a5f4c4)==0x3c0209a6;
        const bool select=m.load32(0x09a5f3d0)==0x8e02001c && m.load32(0x09a5f4c4)==0x0e21993e;
        if(boot) observed.screen=MenuState::Screen::Boot;
        else if(select) {
            const auto menu=m.load32(0x09a44da0);
            if(menu>=0x08800000 && !(menu&3) && m.contains(menu,2472)) {
                const auto state=m.load32(menu+12);
                const auto buffer=m.load32(menu+2380);
                observed=selector.step(state,m.load32(menu+8),m.load8(menu+4)==1,
                    buffer>=0x08800000 && !(buffer&3) && m.contains(buffer,0x16a100));
                if(selector.ready() && !verified) { stop("launcher.save.load_changed"); return; }
            }
        }
        if(navigation->done()) {
            buttons=0;
            const bool lobby=m.load32(0x09aa5770)==0x27bdfdc0 && m.load32(0x09aa49f8)==0x27bdfff0 && m.load32(0x09aa5d90)==0x00a08021;
            const bool quest=m.load32(0x09a6b154)==0x106005f7 && m.load32(0x09ac7ad4)==0xa6a202e4 && m.load32(0x09a85bac)==0xa60202e4;
            if(lobby || quest) {
                const auto hunter=m.load32(0x089cc558);
                // Continue copies the chosen slot's 0x1000 payload into the
                // live hunter at +0x4a0. Verify the actual loaded identity too.
                const auto *identity=hunter>=0x08800000 && m.contains(hunter,0x69700)?m.raw_pointer(hunter+0x4a0,0x68):nullptr;
                if(!identity || psprecomp::sha256_bytes(std::span(identity,0x68))!=expected.appearance_key) {
                    stop("launcher.save.load_changed"); return;
                }
                active=false;
                audio::AudioSink::instance().set_output_suppressed(false);
                std::printf("[direct-load] ready slot=%u hunter=%s state=%08X zenny=%u\n",target+1,expected.name.c_str(),hunter,
                    hunter>=0x08800000 && m.contains(hunter,0x69700)?m.load32(hunter+0x696f0):0);
                std::fflush(stdout);
            }
        } else {
            buttons=navigation->step(observed); navigation->finish_frame();
            if(navigation->failed()) stop("launcher.save.load_unexpected");
            else if(buttons && std::getenv("MHFU_LOAD_TRACE")) {
                std::printf("[direct-load] flip=%u screen=%u cursor=%u buttons=%04X\n",flips,unsigned(observed.screen),observed.selected,buttons);
                std::fflush(stdout);
            }
        }
    }
    if(!std::getenv("MHFU_LOAD_TRACE")) return;
    static unsigned frame=0;
    if(++frame%30!=1) return;
    const auto state=m.load32(0x089cc558);
    const auto menu=m.load32(0x09a44da0);
    std::printf("[load-trace] f=%u state=%08X overlay=%08X/%08X menu=%08X",frame,state,m.load32(0x09a5f3d0),m.load32(0x09a5f4c4),menu);
    if(menu>=0x08800000 && !(menu&3) && m.contains(menu,2472))
        std::printf(" index=%u state=%u mode=%u",m.load32(menu+8),m.load32(menu+12),m.load8(menu+4));
    std::printf("\n"); std::fflush(stdout);
}
}
