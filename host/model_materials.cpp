#include "model_materials.hpp"
#include "gpu/texture_pack.hpp"
#include "psprecomp/guest_memory.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cctype>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>

namespace portablekit::fu::model {
void retain_texture_source(Mesh &mesh,const gpu::TextureState &t,const psprecomp::GuestMemory &memory) {
    if(!t.enabled||!t.width||!t.height||t.width>2048||t.height>2048) return;
    const auto fmt=static_cast<unsigned>(t.format);
    constexpr unsigned bits[]={16,16,16,32,4,8,16,32,4,8,8};
    if(fmt>=std::size(bits)) return;
    const auto unit=128/bits[fmt];auto stride=t.buffer_width&0x7ffu&~(unit-1);if(!stride) stride=unit;
    const std::size_t size=(static_cast<std::size_t>(stride)*t.height+std::max(0,int(t.width)-int(stride)))*bits[fmt]/8;
    if(!size||size>16*1024*1024) return;
    const auto *raw=memory.raw_pointer(t.address&0x0ffffff0,size);if(!raw) return;
    mesh.texture_state={t.address,t.buffer_width,t.width,t.height,fmt,t.swizzled?1u:0u,t.clut_address,t.clut_format,t.clut_shift,t.clut_mask,t.clut_offset,t.clut_format_word,t.clut_load_bytes,t.clut_max_bytes,0,0};
    mesh.texture_bytes.assign(raw,raw+size);
    if(fmt>=4&&fmt<=7&&t.clut_max_bytes) {
        const auto count=std::min(t.clut_max_bytes,4096u);
        if(const auto *clut=memory.raw_pointer(t.clut_address&0x0ffffff0,count)) mesh.palette_bytes.assign(clut,clut+count);
        else {mesh.texture_bytes.clear();mesh.texture_state={};}
    }
}
bool restore_texture_source(const Mesh &mesh,gpu::TextureState &t,psprecomp::GuestMemory &memory) {
    const auto &s=mesh.texture_state;
    if(mesh.texture_bytes.empty()||mesh.texture_bytes.size()>16*1024*1024||mesh.palette_bytes.size()>4096||!s[2]||!s[3]||s[2]>2048||s[3]>2048||s[4]>10||s[5]>1||s[7]>3||s[8]>31||s[9]>0xff||s[10]>0x1f||s[12]>4096||s[13]>4096) return false;
    constexpr unsigned bits[]={16,16,16,32,4,8,16,32,4,8,8};
    const auto unit=128/bits[s[4]];auto stride=s[1]&0x7ffu&~(unit-1);if(!stride) stride=unit;
    const auto expected=(static_cast<std::size_t>(stride)*s[3]+std::max(0,int(s[2])-int(stride)))*bits[s[4]]/8;
    const auto palette_size=s[4]>=4&&s[4]<=7?s[13]:0;
    if(mesh.texture_bytes.size()!=expected||mesh.palette_bytes.size()!=palette_size) return false;
    if(!memory.contains(s[0]&0x0ffffff0,mesh.texture_bytes.size())||(!mesh.palette_bytes.empty()&&!memory.contains(s[6]&0x0ffffff0,mesh.palette_bytes.size()))) return false;
    t={};t.enabled=true;t.address=s[0];t.buffer_width=s[1];t.width=static_cast<std::uint16_t>(s[2]);t.height=static_cast<std::uint16_t>(s[3]);t.format=static_cast<gpu::TextureFormat>(s[4]);t.swizzled=s[5]!=0;t.clut_address=s[6];t.clut_format=s[7];t.clut_shift=s[8];t.clut_mask=s[9];t.clut_offset=s[10];t.clut_format_word=s[11];t.clut_load_bytes=s[12];t.clut_max_bytes=s[13];
    memory.copy_in(t.address&0x0ffffff0,mesh.texture_bytes);
    if(!mesh.palette_bytes.empty()) memory.copy_in(t.clut_address&0x0ffffff0,mesh.palette_bytes);
    return true;
}
namespace {
using Stamp=std::filesystem::file_time_type;
Stamp stamp(const std::filesystem::path &p) {std::error_code ec;const auto s=std::filesystem::last_write_time(p,ec);return ec?Stamp{}:s;}
}
struct MaterialLoader::Impl {
    struct Job {unsigned slot{};std::uint64_t revision{},ticket{};bool enabled{};std::filesystem::path folder;std::shared_ptr<const Scene> original;};
    std::string game_id;
    bool enabled{};std::filesystem::path folder;std::uint64_t revision{1};
    std::map<std::filesystem::path,Stamp> watches;
    std::chrono::steady_clock::time_point next_poll{};
    std::mutex mutex;std::condition_variable_any wake;
    std::array<std::optional<Job>,3> pending;
    std::array<std::optional<MaterialResult>,3> results;
    std::array<std::atomic<std::uint64_t>,3> latest{};
    std::atomic<std::uint64_t> current_revision{1};
    std::jthread worker;
    explicit Impl(std::string id):game_id(std::move(id)),worker([this](std::stop_token stop){run(stop);}) {}
    ~Impl(){worker.request_stop();wake.notify_all();}
    void watch(const std::filesystem::path &p) {if(!watches.contains(p)) watches.emplace(p,stamp(p));}
    void run(std::stop_token stop) {
        std::unique_ptr<gpu::TexturePack> pack;std::uint64_t pack_revision{};
        psprecomp::GuestMemory memory;
        while(!stop.stop_requested()) {
            Job job;
            {std::unique_lock lock(mutex);wake.wait(lock,stop,[&]{return std::any_of(pending.begin(),pending.end(),[](const auto &p){return p.has_value();});});if(stop.stop_requested()) return;
             for(auto &p:pending) if(p) {job=std::move(*p);p.reset();break;}}
            const auto cancelled=[&]{return stop.stop_requested()||latest[job.slot]!=job.ticket||current_revision!=job.revision;};
            if(cancelled()) continue;
            MaterialResult result;result.revision=job.revision;result.ticket=job.ticket;result.scene=*job.original;
            if(job.enabled) {
                if(pack_revision!=job.revision) {pack.reset();std::string error;pack=gpu::TexturePack::open(job.folder,game_id,error);pack_revision=job.revision;}
                if(pack) for(auto &mesh:result.scene.meshes) {
                    if(cancelled()) break;
                    gpu::TextureState state;if(!restore_texture_source(mesh,state,memory)) continue;
                    auto replacement=pack->find(memory,state,0);if(!replacement) continue;
                    result.dependencies.push_back(replacement->file);
                    // This pack instance belongs only to this worker. Release
                    // decoded bytes after each copy, and re-request if another
                    // card needs the same file, rather than retaining HD atlases.
                    if(replacement->state.load(std::memory_order_acquire)==gpu::Replacement::State::Resident)
                        replacement->state.store(gpu::Replacement::State::Unloaded,std::memory_order_release);
                    pack->request(*replacement);
                    while(replacement->state.load(std::memory_order_acquire)==gpu::Replacement::State::Queued&&!cancelled()) std::this_thread::sleep_for(std::chrono::milliseconds(5));
                    if(replacement->state.load(std::memory_order_acquire)!=gpu::Replacement::State::Decoded) continue;
                    const auto w=replacement->width,h=replacement->height;
                    if(!w||!h||w>8192||h>8192||replacement->pixels.size()!=static_cast<std::size_t>(w)*h*4) {pack->consumed(*replacement);continue;}
                    // Bound large source art for these small cards; UVs stay normalized.
                    const float scale=std::min({1.f,2048.f/w,2048.f/h});
                    mesh.width=std::max(1u,static_cast<unsigned>(w*scale));mesh.height=std::max(1u,static_cast<unsigned>(h*scale));
                    mesh.pixels.resize(static_cast<std::size_t>(mesh.width)*mesh.height);
                    for(unsigned y=0;y<mesh.height;++y) for(unsigned x=0;x<mesh.width;++x) {
                        const auto at=(static_cast<std::size_t>(y)*h/mesh.height*w+static_cast<std::size_t>(x)*w/mesh.width)*4;
                        std::memcpy(&mesh.pixels[static_cast<std::size_t>(y)*mesh.width+x],replacement->pixels.data()+at,4);
                    }
                    ++result.replaced;
                    pack->consumed(*replacement);
                }
            }
            if(!cancelled()) {std::lock_guard lock(mutex);results[job.slot]=std::move(result);}
        }
    }
};
MaterialLoader::MaterialLoader(std::string id):impl_(std::make_unique<Impl>(std::move(id))){}
MaterialLoader::~MaterialLoader()=default;
std::uint64_t MaterialLoader::configure(bool enabled,const std::filesystem::path &folder) {
    auto &p=*impl_;bool changed=enabled!=p.enabled||folder!=p.folder;
    if(changed) {p.enabled=enabled;p.folder=folder;p.watches.clear();p.watch(folder);p.watch(folder/"textures.ini");}
    const auto now=std::chrono::steady_clock::now();
    if(now>=p.next_poll) {for(auto &[path,old]:p.watches) {const auto current=stamp(path);if(current!=old){old=current;changed=true;}}p.next_poll=now+std::chrono::milliseconds(500);}
    if(changed) {
        // Pick up newly introduced [games] override files as packs are edited.
        std::error_code ec;for(std::filesystem::directory_iterator it(folder,ec);!ec&&it!=std::filesystem::directory_iterator();it.increment(ec)) {
            auto ext=it->path().extension().string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            if(ext==".ini") p.watch(it->path());
        }
        ++p.revision;p.current_revision=p.revision;
    }
    return p.revision;
}
void MaterialLoader::request(unsigned slot,std::uint64_t ticket,std::shared_ptr<const Scene> original) {
    if(slot>=3||!original) return;auto &p=*impl_;
    std::lock_guard lock(p.mutex);p.latest[slot]=ticket;p.pending[slot]=Impl::Job{slot,p.revision,ticket,p.enabled,p.folder,std::move(original)};p.results[slot].reset();p.wake.notify_one();
}
std::optional<MaterialResult> MaterialLoader::take(unsigned slot,std::uint64_t ticket) {
    if(slot>=3) return {};auto &p=*impl_;std::optional<MaterialResult> result;
    {std::lock_guard lock(p.mutex);if(p.results[slot]) {result=std::move(p.results[slot]);p.results[slot].reset();}}
    if(!result||result->ticket!=ticket||result->revision!=p.revision) return {};
    for(const auto &path:result->dependencies) p.watch(path);
    return result;
}
}
