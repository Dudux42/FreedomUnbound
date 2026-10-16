#include "hunter_model.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
using namespace portablekit::fu::model;
void require(bool v,const char *why) { if(!v) {std::cerr<<why<<'\n';std::exit(1);} }
int main() {
    Scene scene; Mesh far,near;
    far.vertices={{-1,-1,0,0,0,0,0,0,0xff0000ff},{1,-1,0,0,0,0,0,0,0xff0000ff},{0,1,0,0,0,0,0,0,0xff0000ff}};
    near=far; for(auto &v:near.vertices) {v.z=.5f;v.color=0xff00ff00;}
    scene.meshes={near,far};
    auto pixels=render(scene,100,100,0);
    require(pixels[50*100+50]==0xff00ff00,"depth test must retain nearer geometry even when drawn first");
    require(pixels[0]==0,"outside model must be transparent");
    require(render(scene,100,100,1.0f)!=pixels,"turning must project actual 3D geometry");
    near.width=near.height=1;near.pixels={0xffff0000};near.texturing=3;scene.meshes={near};
    require(render(scene,100,100,0)[5050]==0xffff0000,"texture must replace color for replace material");
    near.pixels={0};scene.meshes={near};require(render(scene,100,100,0)[5050]==0,"transparent texture must be discarded");
    const auto file=std::filesystem::current_path()/"fu-hunter-model-test.fum";
    require(write(file,scene),"write scene");Scene reopened;
    require(read(file,reopened)&&reopened.meshes.size()==1,"read model cache");
    require(render(scene,100,100,0)==render(reopened,100,100,0),"cache round trip must preserve rendering");
    { // A real FUM1 entry, before source-texture identity was added.
        std::ofstream out(file,std::ios::binary|std::ios::trunc);
        const auto u32=[&](std::uint32_t value){out.write(reinterpret_cast<const char *>(&value),4);};
        const auto &old=scene.meshes[0];u32(0x314d5546);u32(1);u32(static_cast<std::uint32_t>(old.vertices.size()));
        u32(old.width);u32(old.height);u32(old.wrap_u);u32(old.wrap_v);u32(old.texturing);u32(old.alpha_reference);
        out.write(reinterpret_cast<const char *>(old.vertices.data()),old.vertices.size()*sizeof(Vertex));
        out.write(reinterpret_cast<const char *>(old.pixels.data()),old.pixels.size()*4);
    }
    require(read(file,reopened)&&reopened.meshes[0].texture_bytes.empty(),"legacy FUM1 remains readable");
    require(render(scene,100,100,0)==render(reopened,100,100,0),"legacy cache renders unchanged");
    {std::ofstream out(file,std::ios::binary|std::ios::trunc);out<<"FUM1\xff\xff\xff\xff";}
    require(!read(file,reopened)&&reopened.meshes.empty(),"reject oversized corrupt mesh count");
    require(write(file,scene),"restore fixture");std::filesystem::resize_file(file,35);
    require(!read(file,reopened),"reject truncated cache");
    scene.meshes[0].vertices[0].x=std::numeric_limits<float>::quiet_NaN();
    require(write(file,scene)&&!read(file,reopened),"reject nonfinite cache geometry");
    std::error_code ec;std::filesystem::remove(file,ec);
    require(render(scene,0,100,0).empty(),"reject invalid target dimensions");
    std::cout<<"Hunter model depth, rotation, texture, transparency and cache checks passed.\n";
}
