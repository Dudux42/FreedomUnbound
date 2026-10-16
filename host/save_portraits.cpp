#include "save_portraits.hpp"
#include "equipment_preview.hpp"
#include "direct_load.hpp"
#include "modern_camera.hpp"
#include "hunter_model.hpp"
#include "model_materials.hpp"
#include "profile.hpp"
#include "psprecomp/runtime.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <limits>
#if defined(PORTABLEKIT_HAS_RENDERER)
#include "ui/layer.hpp"
#include "save_data/save_transfer.hpp"
#include "gpu/vulkan_renderer.hpp"
#include "gpu/texture_decode.hpp"
#include "ui/host_text.hpp"
#include "install/user_data.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP
#include "../portablekit/third_party/stb_image.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../portablekit/third_party/stb_image_write.h"
#endif
namespace portablekit::fu {
namespace {
SavePreview loaded;
#if defined(PORTABLEKIT_HAS_RENDERER)
model::Scene captured;
std::vector<std::uint64_t> capture_materials;
bool capturing=false;
std::filesystem::path capture_path;
unsigned capture_slot=3;
bool model_enabled() { const auto *v=std::getenv("MHFU_MODEL_PREVIEW"); return !v || std::strcmp(v,"0"); }
std::array<float,3> transformed(const std::array<float,16> &m,const std::array<float,3> &v,float w) {
    return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2]+m[12]*w,m[1]*v[0]+m[5]*v[1]+m[9]*v[2]+m[13]*w,m[2]*v[0]+m[6]*v[1]+m[10]*v[2]+m[14]*w};
}
void observe_model(const gpu::DrawCall &call,const psprecomp::GuestMemory &memory) {
    if(call.through||call.clear_mode||call.vertices.empty()||captured.meshes.size()>=512) return;
    using P=gpu::PrimitiveType;
    if(call.primitive!=P::Triangles&&call.primitive!=P::TriangleStrip&&call.primitive!=P::TriangleFan) return;
    model::Mesh mesh;
    mesh.width=call.texture.enabled?call.texture.width:0; mesh.height=call.texture.enabled?call.texture.height:0;
    mesh.wrap_u=call.texture.wrap_s; mesh.wrap_v=call.texture.wrap_t; mesh.texturing=call.texture.function;
    mesh.alpha_reference=call.alpha_test.enabled?call.alpha_test.reference:0;
    if(mesh.width&&(mesh.width>2048||mesh.height>2048)) return;
    const auto material=call.texture.enabled?gpu::texture_key(memory,call.texture):0;
    std::size_t match=captured.meshes.size();
    for(std::size_t i=0;i<captured.meshes.size();++i) {
        const auto &a=captured.meshes[i];
        if(capture_materials[i]==material&&a.width==mesh.width&&a.height==mesh.height&&a.wrap_u==mesh.wrap_u&&a.wrap_v==mesh.wrap_v&&a.texturing==mesh.texturing&&a.alpha_reference==mesh.alpha_reference) {match=i;break;}
    }
    if(match==captured.meshes.size()&&mesh.width) {
        if(!gpu::decode_texture(memory,call.texture,mesh.pixels)) return;
        model::retain_texture_source(mesh,call.texture,memory);
    }
    const auto count=call.indices.empty()?call.vertices.size():call.indices.size();
    const auto append=[&](std::size_t i) {
        const auto index=call.indices.empty()?i:call.indices[i]; if(index>=call.vertices.size()) return false;
        const auto &v=call.vertices[index];
        const auto p=transformed(call.view,transformed(call.world,{v.position[0],v.position[1],v.position[2]},1),1);
        const auto n=transformed(call.view,transformed(call.world,v.normal,0),0);
        // Keep vertex/material tint (including armor pigment), light independently.
        auto tint=call.has_vertex_color?v.color:call.material_color;
        if(call.lighting_enabled) {
            const auto ambient=call.has_vertex_color&&(call.lighting.material_update&1)?v.color:call.material_color;
            const auto diffuse=call.has_vertex_color&&(call.lighting.material_update&2)?v.color:call.lighting.material_diffuse;
            tint=0xff000000;
            for(unsigned j=0;j<3;++j) {
                const float value=((ambient>>(j*8))&255)*.35f+((diffuse>>(j*8))&255)*.65f+((call.lighting.material_emissive>>(j*8))&255);
                tint|=static_cast<std::uint32_t>(std::min(value,255.f))<<(j*8);
            }
        }
        mesh.vertices.push_back({p[0],p[1],p[2],n[0],n[1],n[2],v.texcoord[0]*call.texture.scale_u+call.texture.offset_u,v.texcoord[1]*call.texture.scale_v+call.texture.offset_v,tint});
        return true;
    };
    for(std::size_t i=0;i+2<count;i+=call.primitive==P::Triangles?3:1) {
        const auto old=mesh.vertices.size();
        const std::size_t a=call.primitive==P::TriangleFan?0:i;
        const std::size_t b=i+1;
        if(!append(a)||!append(b)||!append(i+2)) { mesh.vertices.resize(old); continue; }
    }
    if(std::getenv("MHFU_PORTRAIT_TRACE")&&!mesh.vertices.empty()) {
        float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
        for(const auto &v:mesh.vertices) {const float p[]={v.x,v.y,v.z};for(int j=0;j<3;++j){lo[j]=std::min(lo[j],p[j]);hi[j]=std::max(hi[j],p[j]);}}
        std::printf("[model] draw=%zu vertices=%zu lit=%u depth=%u blend=%u texture=%08X size=%ux%u bounds=(%.2f %.2f %.2f)-(%.2f %.2f %.2f)\n",captured.meshes.size(),mesh.vertices.size(),call.lighting_enabled,call.depth.test_enabled,call.blend.enabled,call.texture.address,mesh.width,mesh.height,lo[0],lo[1],lo[2],hi[0],hi[1],hi[2]);
    }
    if(!mesh.vertices.empty()) {
        if(match<captured.meshes.size()) {
            auto &vertices=captured.meshes[match].vertices;
            if(vertices.size()+mesh.vertices.size()<=200000) vertices.insert(vertices.end(),mesh.vertices.begin(),mesh.vertices.end());
        } else { captured.meshes.push_back(std::move(mesh)); capture_materials.push_back(material); }
    }
}
std::filesystem::path picture(unsigned slot,const std::string &key) {
    if(slot>=3 || key.size()!=64 || key.find_first_not_of("0123456789abcdef")!=std::string::npos) return {};
    return preview_file(savedata::memory_stick()).parent_path()/"portraits"/("slot-"+std::to_string(slot+1)+"-"+key+".png");
}
std::vector<unsigned char> read_image(const std::filesystem::path &path) {
    std::error_code ec; const auto size=std::filesystem::file_size(path,ec);
    if(ec || size>128*1024*1024) return {};
    std::ifstream in(path,std::ios::binary); return {std::istreambuf_iterator<char>(in),{}};
}
void png_chunk(void *context,void *data,int size) {
    static_cast<std::ofstream *>(context)->write(static_cast<const char *>(data),size);
}
bool capture_portrait(const std::filesystem::path &target) {
    auto &renderer=ui::Layer::get().renderer();
    std::error_code ec; std::filesystem::create_directories(target.parent_path(),ec); if(ec) return false;
    auto bmp=target; bmp+=".capture.bmp";
    if(!renderer.capture_frame(install::path_to_utf8(bmp))) return false;
    const auto bytes=read_image(bmp); std::filesystem::remove(bmp,ec);
    int w=0,h=0,n=0;
    auto *pixels=bytes.empty()?nullptr:stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),&w,&h,&n,4);
    if(!pixels || w<480 || h<272) { stbi_image_free(pixels); return false; }
    // Character Select's model panel, excluding the slot list and bottom prompt.
    const int x=w*52/100,y=h*19/100,cw=w*44/100,ch=h*57/100;
    std::vector<unsigned char> crop(static_cast<std::size_t>(cw)*ch*4);
    for(int row=0;row<ch;++row) std::copy_n(pixels+((y+row)*w+x)*4,cw*4,crop.data()+row*cw*4);
    // The game's Rotate hint occupies this corner above the model panel.
    for(int row=0;row<ch;++row) for(int col=0;col<cw;++col)
        if(y+row<h*31/100 && x+col>=w*82/100) crop[(row*cw+col)*4+3]=0;
    stbi_image_free(pixels);
    auto temp=target; temp+=".tmp";
    std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    const bool encoded=stbi_write_png_to_func(png_chunk,&out,cw,ch,4,crop.data(),cw*4)!=0;
    out.flush(); const bool good=encoded && bool(out); out.close();
    if(!good) { std::filesystem::remove(temp,ec); return false; }
    std::filesystem::rename(temp,target,ec);
    if(ec) { std::filesystem::remove(temp,ec); return false; }
    return true;
}
#endif
void frame(psprecomp::Runtime &runtime) {
    direct_load::frame(runtime);
    if(modern_camera::driver.frame) modern_camera::driver.frame(runtime);
#if defined(PORTABLEKIT_HAS_RENDERER)
    const auto &m=runtime.memory();
    if(capturing) {
        ui::Layer::get().renderer().set_draw_observer({}); capturing=false;
        const auto menu=m.load32(0x09a44da0);
        if(m.contains(menu,2472)&&m.load32(menu+8)==capture_slot&&m.load32(menu+12)==3&&model::write(capture_path,captured))
            std::printf("[model] saved slot=%u meshes=%zu\n",capture_slot+1,captured.meshes.size());
        captured={}; capture_materials.clear();
    }
    // Texture-only observation retains the renderer's GPU vertex fast path.
    if(ui::Layer::get().attached()) {
        if(equipment::needs_icons())
            ui::Layer::get().renderer().set_draw_observer(equipment::observe,false);
        else ui::Layer::get().renderer().set_draw_observer({});
    }
    // Continue task identity. Other overlays reuse this address window.
    static unsigned stable=0,last_slot=3,last_state=0;
    if(m.load32(0x09a5f3d0)!=0x8e02001c || m.load32(0x09a5f4c4)!=0x0e21993e) { stable=0; last_slot=3; return; }
    const auto menu=m.load32(0x09a44da0);
    if((menu&3) || menu<0x08800000 || !m.contains(menu,2472)) return;
    static unsigned frames{};
    if(std::getenv("MHFU_PORTRAIT_TRACE") && ++frames%15==1) {
        std::printf("[portrait] menu=%08X index=%u state=%u model=%08X mode=%u\n",menu,m.load32(menu+8),m.load32(menu+12),m.load32(menu+2380),m.load8(menu+4));
        std::fflush(stdout);
    }
    const unsigned slot=m.load32(menu+8),state=m.load32(menu+12);
    if(slot!=last_slot || state!=last_state) { stable=0; last_slot=slot; last_state=state; }
    // State 3 is the settled Character Select preview, before confirmation.
    if(state!=3 || m.load8(menu+4)!=1 || !m.load32(menu+2380)) { stable=0; return; }
    if(slot>=3 || loaded.status!=PreviewStatus::Ready || !loaded.slots[slot].occupied || !ui::Layer::get().attached()) return;
    if(++stable!=45) return;
    const auto target=picture(slot,loaded.slots[slot].appearance_key);
    if(target.empty()) return;
    if(capture_portrait(target)) std::printf("[portrait] captured slot=%u state=%u\n",slot+1,state);
    if(model_enabled()) {
        captured={}; capture_materials.clear(); capture_slot=slot; capture_path=target; capture_path.replace_extension(".fum");
        capturing=true; ui::Layer::get().renderer().set_draw_observer([](const auto &call,const auto &memory) {
            equipment::observe(call,memory);observe_model(call,memory);
        });
    }
#endif
}
}
const CameraDriver *portrait_camera_driver() {
    static const CameraDriver driver=[] { auto d=modern_camera::driver; d.frame=frame; return d; }();
    return &driver;
}
void portrait_save_seen(const SavePreview &preview) { loaded=preview; }
void draw_save_portrait(unsigned slot,const HunterSummary &hunter,float x,float y,float width,float height) {
#if defined(PORTABLEKIT_HAS_RENDERER)
    if(slot>=3 || width<=0 || height<=0) return;
    struct Cached { std::filesystem::path file; ImTextureData *texture{}; std::filesystem::file_time_type stamp{}; model::Scene scene; float yaw{},rendered_yaw{1000}; int render_width{},render_height{}; std::shared_ptr<const model::Scene> original; std::uint64_t materials_revision{},ticket{}; };
    static std::array<Cached,3> cache;
    static std::vector<ImTextureData *> retired;
    static model::MaterialLoader materials(game().disc_id);
    static std::uint64_t next_ticket{};
    auto &renderer=ui::Layer::get().renderer();
    const bool hd_enabled=settings::current().texture_pack&&!renderer.texture_pack_held();
    const auto materials_revision=materials.configure(hd_enabled,install::path_from_utf8(renderer.texture_pack_folder()));
    for(auto it=retired.begin();it!=retired.end();) {
        if((*it)->Status==ImTextureStatus_Destroyed) { ImGui::UnregisterUserTexture(*it); delete *it; it=retired.erase(it); }
        else ++it;
    }
    auto &c=cache[slot]; auto path=picture(slot,hunter.appearance_key);
    std::error_code ec;
    if(model_enabled()) {
        auto mesh=path; mesh.replace_extension(".fum");
        if(std::filesystem::is_regular_file(mesh,ec)) path=mesh;
    }
    const auto stamp=std::filesystem::last_write_time(path,ec);
    if(c.file!=path || (!ec && c.stamp!=stamp)) {
        if(c.texture) { c.texture->WantDestroyNextFrame=true; retired.push_back(c.texture); }
        c={path,nullptr,stamp};
        if(path.extension()==".fum"&&model::read(path,c.scene)) c.original=std::make_shared<const model::Scene>(c.scene);
        if(c.scene.meshes.empty()) {
        const auto bytes=read_image(path); int w=0,h=0,n=0;
        auto *rgba=bytes.empty()?nullptr:stbi_load_from_memory(bytes.data(),static_cast<int>(bytes.size()),&w,&h,&n,4);
        if(rgba && w>0 && h>0 && w<=4096 && h<=4096) {
            c.texture=new ImTextureData; c.texture->Create(ImTextureFormat_RGBA32,w,h);
            std::memcpy(c.texture->GetPixels(),rgba,static_cast<std::size_t>(w)*h*4);
            ImGui::RegisterUserTexture(c.texture);
        }
        stbi_image_free(rgba);
        }
    }
    auto *draw=ImGui::GetBackgroundDrawList();
    draw->AddRectFilled({x,y},{x+width,y+height},IM_COL32(8,15,21,210),5);
    if(!c.scene.meshes.empty()) {
        if(c.original&&c.materials_revision!=materials_revision) {
            c.materials_revision=materials_revision;c.ticket=++next_ticket;
            c.scene=*c.original;c.rendered_yaw=std::numeric_limits<float>::quiet_NaN();
            if(hd_enabled) materials.request(slot,c.ticket,c.original);
        }
        if(auto result=materials.take(slot,c.ticket)) {
            c.scene=std::move(result->scene);c.rendered_yaw=std::numeric_limits<float>::quiet_NaN();
            if(std::getenv("MHFU_PORTRAIT_TRACE")) std::printf("[model-materials] slot=%u replaced=%u/%zu\n",slot+1,result->replaced,c.scene.meshes.size());
        }
        auto &io=ImGui::GetIO();
        // Let ImGui retain the original card's active ID until release. A
        // hover-only check hands the same held drag to whichever card it enters.
        const auto cursor=ImGui::GetCursorScreenPos();
        ImGui::PushID(static_cast<int>(slot));
        ImGui::SetCursorScreenPos({x,y});
        ImGui::InvisibleButton("##hunter_rotate",{width,height},ImGuiButtonFlags_MouseButtonLeft);
        const bool dragging=ImGui::IsItemActive();
        if(ImGui::IsItemHovered()||dragging) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
        if(dragging&&ImGui::IsMouseDragging(ImGuiMouseButton_Left,0.0f)) c.yaw+=io.MouseDelta.x*.012f;
        ImGui::PopID();
        ImGui::SetCursorScreenPos(cursor);
        if(const auto *angle=std::getenv("MHFU_MODEL_YAW")) c.yaw=static_cast<float>(std::atof(angle));
        // Exercise consecutive texture uploads in bounded native diagnostics.
        if(const auto *speed=std::getenv("MHFU_MODEL_SPIN_TEST")) c.yaw=static_cast<float>(ImGui::GetTime()*std::atof(speed));
        const int rw=std::clamp(static_cast<int>(width*2),64,768),rh=std::clamp(static_cast<int>(height*2),64,768);
        if(c.yaw!=c.rendered_yaw||rw!=c.render_width||rh!=c.render_height) {
            if(std::getenv("MHFU_PORTRAIT_TRACE")&&(rw!=c.render_width||rh!=c.render_height))
                std::printf("[model-view] slot=%u panel=%.1fx%.1f texture=%dx%d meshes=%zu\n",slot+1,width,height,rw,rh,c.scene.meshes.size());
            const auto image=model::render(c.scene,rw,rh,c.yaw);
            // Rotation updates one persistent texture to avoid GPU allocation churn.
            if(!c.texture||c.texture->Width!=rw||c.texture->Height!=rh) {
                if(c.texture) {c.texture->WantDestroyNextFrame=true;retired.push_back(c.texture);}
                c.texture=new ImTextureData; c.texture->Create(ImTextureFormat_RGBA32,rw,rh);
                ImGui::RegisterUserTexture(c.texture);
            }
            std::memcpy(c.texture->GetPixels(),image.data(),image.size()*4);
            ImTextureDataQueueUpload(c.texture,0,0,rw,rh);
            c.rendered_yaw=c.yaw;
            c.render_width=rw;c.render_height=rh;
        }
    }
    // An unuploaded texture has no usable descriptor. Keep the placeholder for
    // its first frame rather than submitting an invalid texture reference.
    if(std::getenv("MHFU_PORTRAIT_TRACE")&&ImGui::GetFrameCount()%30==0&&c.texture)
        std::printf("[model-texture] slot=%u ptr=%p status=%d id=%llu retire=%u registered=%u\n",slot+1,static_cast<void *>(c.texture),static_cast<int>(c.texture->Status),static_cast<unsigned long long>(c.texture->GetTexID()),c.texture->WantDestroyNextFrame,ImGui::GetCurrentContext()->UserTextures.contains(c.texture));
    if(c.texture&&c.texture->GetTexID()!=ImTextureID_Invalid) {
        const float scale=std::min(width/c.texture->Width,height/c.texture->Height);
        const float w=c.texture->Width*scale,h=c.texture->Height*scale;
        draw->AddImage(c.texture->GetTexRef(),{x+(width-w)*.5f,y+(height-h)*.5f},{x+(width+w)*.5f,y+(height+h)*.5f});
    } else {
        const float radius=std::min(width*.12f,height*.13f);
        const ImVec2 center{x+width*.5f,y+height*.37f};
        draw->AddCircleFilled(center,radius,IM_COL32(57,76,87,255));
        draw->AddRectFilled({center.x-radius*1.7f,center.y+radius*1.3f},{center.x+radius*1.7f,center.y+radius*3.4f},IM_COL32(57,76,87,255),radius);
        const auto text=ui::host_text::get("launcher.save.portrait_pending");
        const float size=std::min(ImGui::GetFontSize()*.7f,width/12);
        draw->AddText(ImGui::GetFont(),size,{x+width*.08f,y+height-size*2},IM_COL32(160,177,187,255),text.c_str(),nullptr,width*.84f);
    }
#endif
}
}
