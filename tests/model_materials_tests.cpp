#include "model_materials.hpp"
#include "gpu/texture_pack.hpp"
#include "perf/frame_stats.hpp"
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
bool portablekit::perf::alternate_off(NewPath) {return false;}
namespace m=portablekit::fu::model;
namespace g=portablekit::gpu;
namespace fs=std::filesystem;
void require(bool value,const char *why) {if(!value){std::cerr<<why<<'\n';std::exit(1);}}
void png(const fs::path &file,std::uint32_t color) {
    std::vector<std::uint32_t> pixels(32*32,color);std::ofstream out(file,std::ios::binary);
    require(stbi_write_png_to_func([](void *c,void *data,int n){static_cast<std::ofstream *>(c)->write(static_cast<char *>(data),n);},&out,32,32,4,pixels.data(),32*4)!=0,"write synthetic replacement");
}
m::MaterialResult wait(m::MaterialLoader &loader,unsigned slot,std::uint64_t ticket) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);
    while(std::chrono::steady_clock::now()<end) {if(auto r=loader.take(slot,ticket)) return std::move(*r);std::this_thread::sleep_for(std::chrono::milliseconds(5));}
    require(false,"material worker timed out");return {};
}
int main(int argc,char **argv) {
    if(argc==3) {
        auto scene=std::make_shared<m::Scene>();require(m::read(argv[1],*scene),"read real model cache");
        m::MaterialLoader loader("ULUS10391");loader.configure(true,argv[2]);loader.request(0,1,scene);
        const auto result=wait(loader,0,1);std::cout<<"matched="<<result.replaced<<" materials="<<result.scene.meshes.size()<<'\n';
        for(const auto &p:result.dependencies) std::cout<<p.string()<<'\n';return 0;
    }
    const auto root=fs::current_path()/("model-materials-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root/"pack");fs::create_directories(root/"other");
    {
        psprecomp::GuestMemory live,restored;
        g::TextureState texture;texture.enabled=true;texture.address=0x08850000;texture.buffer_width=16;texture.width=16;texture.height=8;texture.format=g::TextureFormat::Rgba8888;
        for(unsigned at=0;at<16*8*4;at+=4) live.store32(texture.address+at,0xff203040);
        m::Mesh mesh;mesh.width=16;mesh.height=8;mesh.pixels.assign(128,0xff203040);mesh.vertices={{-1,-1,0},{1,-1,0},{0,1,0}};
        m::retain_texture_source(mesh,texture,live);
        g::TextureState recovered;require(m::restore_texture_source(mesh,recovered,restored),"restore direct texture identity");
        for(bool ignore:{false,true}) for(auto hash:{g::TexturePackHash::Xxh32,g::TexturePackHash::Xxh64}) {
            g::TexturePackOptions options;options.ignore_address=ignore;options.hash=hash;options.reduce_hash=true;
            g::TexturePackKey a,b;std::uint32_t w,h;
            require(g::compute_texture_pack_key(live,texture,0,options,a,w,h)&&g::compute_texture_pack_key(restored,recovered,0,options,b,w,h)&&a==b,"cache identity must match live texture across pack options");
        }
        g::TexturePackOptions options;options.ignore_address=true;g::TexturePackKey direct;std::uint32_t w,h;
        require(g::compute_texture_pack_key(live,texture,0,options,direct,w,h),"derive replacement mapping");
        auto palette=texture;palette.address=0x08852000;palette.clut_address=0x08853000;palette.format=g::TextureFormat::Clut4;palette.buffer_width=32;palette.clut_format=3;palette.clut_format_word=0xc5000003;palette.clut_mask=15;palette.clut_load_bytes=64;palette.clut_max_bytes=64;
        for(unsigned i=0;i<128;++i) live.store8(palette.address+i,static_cast<std::uint8_t>(i));
        for(unsigned i=0;i<16;++i) live.store32(palette.clut_address+i*4,0xff000000+i*0x010101);
        auto second=mesh;m::retain_texture_source(second,palette,live);
        require(m::restore_texture_source(second,recovered,restored),"restore paletted texture identity");
        g::TexturePackKey a,b;require(g::compute_texture_pack_key(live,palette,0,options,a,w,h)&&g::compute_texture_pack_key(restored,recovered,0,options,b,w,h)&&a==b,"palette bytes must preserve texture-pack hash");
        auto scene=std::make_shared<m::Scene>();scene->meshes={mesh,second};
        require(m::write(root/"cache.fum",*scene),"write FUM2");m::Scene disk;require(m::read(root/"cache.fum",disk),"read FUM2");
        require(disk.meshes[0].texture_bytes==mesh.texture_bytes&&disk.meshes[1].palette_bytes==second.palette_bytes,"source bytes survive serialization");
        const auto ini="[options]\nhash=xxh64\nignoreAddress=true\n[hashes]\n"+g::format_texture_pack_key(direct)+"=armor.png\n";
        std::ofstream(root/"pack/textures.ini")<<ini;std::ofstream(root/"other/textures.ini")<<ini;
        png(root/"pack/armor.png",0xff0000ff);png(root/"other/armor.png",0xffff0000);
        m::MaterialLoader loader("ULUS10391");auto revision=loader.configure(true,root/"pack");
        for(unsigned slot=0;slot<3;++slot) loader.request(slot,slot+1,scene);
        for(unsigned slot=0;slot<3;++slot) {auto result=wait(loader,slot,slot+1);require(result.replaced==1&&result.scene.meshes[0].width==32&&result.scene.meshes[0].pixels[0]==0xff0000ff,"all slots apply matching HD material");require(result.scene.meshes[1].pixels==second.pixels,"unmatched material preserves original");}
        require(scene->meshes[0].pixels==mesh.pixels,"base cache remains original");
        png(root/"pack/armor.png",0xff00ff00);fs::last_write_time(root/"pack/armor.png",fs::file_time_type::clock::now()+std::chrono::seconds(2));
        std::this_thread::sleep_for(std::chrono::milliseconds(550));require(loader.configure(true,root/"pack")>revision,"matched image edit invalidates materials");loader.request(0,10,scene);
        require(wait(loader,0,10).scene.meshes[0].pixels[0]==0xff00ff00,"edited HD image refreshes");
        loader.configure(true,root/"other");loader.request(0,11,scene);require(wait(loader,0,11).scene.meshes[0].pixels[0]==0xffff0000,"switch packs");
        loader.request(0,12,scene);loader.configure(false,root/"other");require(!loader.take(0,12),"disable discards stale enabled result");loader.request(0,13,scene);const auto disabled=wait(loader,0,13);require(!disabled.replaced&&disabled.scene.meshes[0].pixels==mesh.pixels,"disable restores original");
        std::ofstream(root/"other/armor.png",std::ios::binary|std::ios::trunc)<<"broken";loader.configure(true,root/"other");loader.request(0,14,scene);require(wait(loader,0,14).replaced==0,"corrupt replacement falls back safely");
        png(root/"other/armor.png",0xffff0000);
        std::ofstream(root/"other/textures.ini")<<"[options]\nhash=xxh64\nignoreAddress=true\n[hashes]\n"<<g::format_texture_pack_key(direct)<<"=\n";
        fs::last_write_time(root/"other/textures.ini",fs::file_time_type::clock::now()+std::chrono::seconds(2));
        std::this_thread::sleep_for(std::chrono::milliseconds(550));loader.configure(true,root/"other");loader.request(0,15,scene);
        require(wait(loader,0,15).replaced==0,"edited INI can explicitly retain original material");
        std::ofstream(root/"other/textures.ini")<<ini;fs::last_write_time(root/"other/textures.ini",fs::file_time_type::clock::now()+std::chrono::seconds(4));
        std::this_thread::sleep_for(std::chrono::milliseconds(550));loader.configure(true,root/"other");loader.request(0,16,scene);
        require(wait(loader,0,16).scene.meshes[0].pixels[0]==0xffff0000,"edited INI restores replacement mapping");
        auto truncated=mesh;truncated.texture_bytes.pop_back();require(!m::restore_texture_source(truncated,recovered,restored),"reject truncated source bytes");
        second.texture_state[0]=0xffffffff;require(!m::restore_texture_source(second,recovered,restored),"reject invalid cached address");
    }
    // The resolved directory was created by this test inside its working directory.
    require(root.parent_path()==fs::current_path(),"cleanup stays in test directory");fs::remove_all(root);
    std::cout<<"Material identity, all slots, fallback, edit refresh, pack switching and disable checks passed.\n";
}
