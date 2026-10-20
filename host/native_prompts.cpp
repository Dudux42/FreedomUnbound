#include "native_prompts.hpp"
#include "native_prompt_model.hpp"
#include "native_prompt_draw.hpp"
#include "controller_prompts.hpp"
#include "gpu/ge_state.hpp"
#include "gpu/texture_decode.hpp"
#include "psprecomp/guest_memory.hpp"
#include <atomic>
#include <map>
#include <set>
#include <cstdio>
#include <cstdlib>
#include <array>
namespace portablekit::fu::controller_prompts {
namespace {
std::atomic<bool> active{};
std::map<std::uint64_t,int> atlases;
std::uint64_t strip_hash(const std::vector<std::uint32_t> &p,unsigned width,unsigned x,unsigned y) {
    std::uint64_t h=14695981039346656037ull;
    for(unsigned row=y;row<y+16;++row)for(unsigned col=x;col<x+104;++col)
        {h^=(p[row*width+col]>>24) ? 1u : 0u;h*=1099511628211ull;}
    return h;
}
int atlas(const gpu::TextureState &texture,const psprecomp::GuestMemory &memory) {
    const auto key=gpu::texture_key(memory,texture);
    if(const auto i=atlases.find(key);i!=atlases.end())return i->second;
    std::vector<std::uint32_t> p;int result=-1;
    if(gpu::decode_texture(memory,texture,p) && p.size()>=std::size_t(texture.width)*texture.height) {
        if(strip_hash(p,texture.width,0,0)==0x5d12b898cb46a458ull)result=0;
        else if(texture.width>=208 && texture.height>=160 && strip_hash(p,texture.width,104,144)==0xbe5685f8c9c5304dull)result=1;
    }
    if(atlases.size()>=128)atlases.clear();atlases.emplace(key,result);
    if(result>=0 && std::getenv("MHFU_NATIVE_PROMPTS_TRACE")){
        std::printf("[native-prompts] atlas=%d address=%08X key=%016llX\n",result,texture.address,static_cast<unsigned long long>(key));std::fflush(stdout);
    }
    return result;
}

}
void enable_native(bool value){active.store(value);}
bool transform_native(const gpu::DrawCall &call,const psprecomp::GuestMemory &memory,std::vector<gpu::DrawCall> &out) {
    if(!active.load() || !call.through || call.clear_mode || !call.texture.enabled || call.vertices.empty() ||
       call.texture.width<104 || call.texture.height<16 || call.texture.width>512 || call.texture.height>512)return false;
    if(std::getenv("MHFU_NATIVE_PROMPTS_TRACE") && call.vertices.size()<=6) {
        const auto &a=call.vertices.front(),&b=call.vertices.back();
        if((a.texcoord[1]<32 && a.texcoord[0]<216) || (a.texcoord[1]>=144 && a.texcoord[1]<160)) {
            const auto sheet=atlas(call.texture,memory);
            if(sheet>=0) {
                static std::set<std::string> seen;
                const auto key=std::to_string(sheet)+":"+std::to_string(unsigned(call.primitive))+":"+std::to_string(a.texcoord[0])+":"+std::to_string(a.texcoord[1])+":"+std::to_string(b.texcoord[0])+":"+std::to_string(b.texcoord[1]);
                if(seen.insert(key).second){std::printf("[native-prompts-probe] sheet=%d primitive=%u count=%zu indices=%zu size=%ux%u uv=%.1f,%.1f..%.1f,%.1f xy=%.1f,%.1f..%.1f,%.1f\n",sheet,unsigned(call.primitive),call.vertices.size(),call.indices.size(),call.texture.width,call.texture.height,a.texcoord[0],a.texcoord[1],b.texcoord[0],b.texcoord[1],a.position[0],a.position[1],b.position[0],b.position[1]);std::fflush(stdout);}
            }
        }
    }
    if(call.primitive!=gpu::PrimitiveType::Sprites || !call.indices.empty() || call.vertices.size()%2)return false;
    bool possible=false;
    for(std::size_t i=0;i+1<call.vertices.size();i+=2){const auto &a=call.vertices[i],&b=call.vertices[i+1];
        if(native_action(0,a.texcoord[0],a.texcoord[1],b.texcoord[0],b.texcoord[1]) ||
           native_action(1,a.texcoord[0],a.texcoord[1],b.texcoord[0],b.texcoord[1]))possible=true;}
    if(!possible)return false;
    const int sheet=atlas(call.texture,memory);if(sheet<0)return false;
    return replace_native_draw(call,sheet,mapped_label,out,current_family());
}
}
