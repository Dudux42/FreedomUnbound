#include "native_prompt_model.hpp"
#include "native_prompt_draw.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
using namespace portablekit;
using namespace portablekit::fu::controller_prompts;
void check(bool value){if(!value){std::cerr<<"Native prompt regression failed\n";std::exit(1);}}
void pair(gpu::DrawCall &c,float u,float v,float x){
    gpu::Vertex a,b;a.texcoord={u,v};b.texcoord={u+12,v+12};
    a.position={x,100,1,1};b.position={x+12,112,1,1};a.color=b.color=0x80ffffffu;
    c.vertices.push_back(a);c.vertices.push_back(b);
}
int main(){
    check(native_action(0,16,0,28,12)==input::Action::Cross);
    check(native_action(1,104,144,116,156)==input::Action::Circle);
    check(native_action(0,0,16,20,24)==input::Action::Start);
    check(native_action(0,20,16,40,24)==input::Action::Select);
    check(native_action(0,104,0,120,16)==input::Action::StickUp);
    check(native_action(0,64,0,84,16)==input::Action::L);
    check(native_action(0,84,0,104,16)==input::Action::R);
    check(native_action(1,208,144,224,160)==input::Action::StickUp);
    check(native_action(0,64,16,80,32)==input::Action::Up);
    check(!native_action(0,0,0,96,16)); // A whole strip is not a button.
    check(!native_action(0,15,0,19,12)); // Crosses a cell boundary.
    check(!native_action(0,0,0,std::numeric_limits<float>::quiet_NaN(),12));
    check(compact_key("Mouse Right")=="MR" && compact_key("Left Shift")=="LS");
    gpu::DrawCall original;original.primitive=gpu::PrimitiveType::Sprites;
    original.through=true;original.texture.enabled=true;original.has_vertex_color=true;
    pair(original,200,40,10);pair(original,16,0,30);pair(original,200,40,50);
    std::vector<gpu::DrawCall> out;
    check(replace_native_draw(original,0,[](auto){return "F8";},out));
    check(out.size()==3 && out[0].texture.enabled && !out[1].texture.enabled && out[2].texture.enabled);
    check(out[0].vertices.size()==2 && out[2].vertices.size()==2 && original.vertices.size()==6);
    check(out[0].vertices[0].position[0]==10 && out[2].vertices[0].position[0]==50);
    check(out[1].primitive==gpu::PrimitiveType::Triangles && !out[1].vertices.empty());
    for(const auto &v:out[1].vertices){check(v.position[0]>=30 && v.position[0]<=42 && v.position[1]>=100 && v.position[1]<=112);check((v.color>>24)==128);}
    original.vertices.erase(original.vertices.begin()+2,original.vertices.begin()+4);out.clear();
    check(!replace_native_draw(original,0,[](auto){return "A";},out) && out.empty());
    for(const char *label:{"Circle","Cross","Square","Triangle","--","MR","LS","D:W|A|S|D"}){
        original.vertices.clear();pair(original,0,0,30);out.clear();
        check(replace_native_draw(original,0,[=](auto){return label;},out) && out.size()==1);
        for(const auto &v:out[0].vertices)check(std::isfinite(v.position[0]+v.position[1]));
    }
    for(const auto &[family,label]:{std::pair{Family::Nintendo,"A"},std::pair{Family::PlayStation,"Cross"},
        std::pair{Family::Xbox,"B"},std::pair{Family::Keyboard,"F8"}}) {
        original.vertices.clear();pair(original,16,0,30);out.clear();
        check(replace_native_draw(original,0,[=](auto){return label;},out,family) && out.size()==1);
        for(const auto &v:out[0].vertices)check(std::isfinite(v.position[0]+v.position[1]));
    }
}
