#include "native_prompt_draw.hpp"
#include "native_prompt_model.hpp"
#include <cmath>
#include <array>
#include <set>
#include <cstdio>
#include <cstdlib>
namespace portablekit::fu::controller_prompts {
namespace {
void triangle(gpu::DrawCall &out,float x0,float y0,float x1,float y1,float x2,float y2,float z,std::uint32_t color) {
    for(auto p:{std::array{x0,y0},std::array{x1,y1},std::array{x2,y2}}){gpu::Vertex v;v.position={p[0],p[1],z,1};v.color=color;out.vertices.push_back(v);}
}
void rect(gpu::DrawCall &out,float x,float y,float w,float h,float z,std::uint32_t color) {
    triangle(out,x,y,x+w,y,x,y+h,z,color);triangle(out,x+w,y,x+w,y+h,x,y+h,z,color);
}
void line(gpu::DrawCall &out,float x0,float y0,float x1,float y1,float thickness,float z,std::uint32_t color) {
    const auto length=std::hypot(x1-x0,y1-y0);if(length<=0)return;
    const float dx=(y1-y0)*thickness/(2*length),dy=(x0-x1)*thickness/(2*length);
    triangle(out,x0-dx,y0-dy,x1-dx,y1-dy,x0+dx,y0+dy,z,color);
    triangle(out,x1-dx,y1-dy,x1+dx,y1+dy,x0+dx,y0+dy,z,color);
}
void disc(gpu::DrawCall &out,float cx,float cy,float r,float z,std::uint32_t color,bool filled,float thickness) {
    constexpr int segments=20;
    for(int i=0;i<segments;++i) {
        const float a=i*6.2831853f/segments,b=(i+1)*6.2831853f/segments;
        const float x0=cx+r*std::cos(a),y0=cy+r*std::sin(a),x1=cx+r*std::cos(b),y1=cy+r*std::sin(b);
        if(filled)triangle(out,cx,cy,x0,y0,x1,y1,z,color);
        else line(out,x0,y0,x1,y1,thickness,z,color);
    }
}
void pixel_text(gpu::DrawCall &out,const std::string &label,float x,float y,float w,float h,float z,std::uint32_t color) {
    const auto text=compact_key(label.empty() ? "--" : label);const float columns=static_cast<float>(text.size()*4-1);
    const float pixel=std::min(w/std::max(columns,1.f),h/5);
    const float tx=x+(w-columns*pixel)*.5f,ty=y+(h-5*pixel)*.5f;
    for(std::size_t n=0;n<text.size();++n){const auto bits=key_pixels(text[n]);
        for(unsigned row=0;row<5;++row)for(unsigned col=0;col<3;++col)
            if(bits&(1u<<(14-row*3-col)))rect(out,tx+(n*4+col)*pixel,ty+row*pixel,pixel,pixel,z,color);}
}
void ps_symbol(gpu::DrawCall &out,const std::string &label,float cx,float cy,float r,float z,std::uint32_t color,float thickness) {
    if(label=="Cross") {line(out,cx-r,cy-r,cx+r,cy+r,thickness,z,color);line(out,cx+r,cy-r,cx-r,cy+r,thickness,z,color);}
    else if(label=="Circle")disc(out,cx,cy,r*.78f,z,color,false,thickness);
    else if(label=="Square") {
        line(out,cx-r*.72f,cy-r*.72f,cx+r*.72f,cy-r*.72f,thickness,z,color);
        line(out,cx+r*.72f,cy-r*.72f,cx+r*.72f,cy+r*.72f,thickness,z,color);
        line(out,cx+r*.72f,cy+r*.72f,cx-r*.72f,cy+r*.72f,thickness,z,color);
        line(out,cx-r*.72f,cy+r*.72f,cx-r*.72f,cy-r*.72f,thickness,z,color);
    } else {
        line(out,cx,cy-r,cx+r,cy+r*.75f,thickness,z,color);
        line(out,cx+r,cy+r*.75f,cx-r,cy+r*.75f,thickness,z,color);
        line(out,cx-r,cy+r*.75f,cx,cy-r,thickness,z,color);
    }
}
gpu::DrawCall cap(const gpu::DrawCall &source,const gpu::Vertex &a,const gpu::Vertex &b,const std::string &label,Family family) {
    auto out=source;out.vertices.clear();out.indices.clear();out.primitive=gpu::PrimitiveType::Triangles;
    out.texture.enabled=false;out.has_vertex_color=true;out.lighting_enabled=false;out.culling_enabled=false;
    out.vertex_type=0;out.raw_vertices=nullptr;out.raw_count=0;
    const float left=std::min(a.position[0],b.position[0]),top=std::min(a.position[1],b.position[1]);
    const float width=std::abs(a.position[0]-b.position[0]),height=std::abs(a.position[1]-b.position[1]);
    const float z=a.position[2],inset=std::min(width,height)*.05f;
    const auto ink=source.has_vertex_color ? b.color : source.material_color;
    const auto alpha=ink&0xff000000u;
    const float x=left+inset,y=top+inset,w=width-2*inset,h=height-2*inset;
    const float cx=x+w*.5f,cy=y+h*.5f,r=std::min(w,h)*.27f;
    if(label.starts_with("D:")) {
        std::size_t start=2;
        const std::array<std::array<float,2>,4> centers{{{cx,cy-h*.31f},{cx-w*.27f,cy},{cx,cy+h*.31f},{cx+w*.27f,cy}}};
        for(unsigned n=0;n<4;++n){const auto end=label.find('|',start);const auto text=compact_key(label.substr(start,end-start));
            const float kw=w*.24f,kh=h*.24f,kx=centers[n][0]-kw*.5f,ky=centers[n][1]-kh*.5f;
            rect(out,kx+inset,ky+inset,kw,kh,z,alpha|0x00182024u);
            rect(out,kx,ky,kw-inset,kh-inset,z,family==Family::Keyboard ? alpha|0x00e8ecee : alpha|0x003c454au);
            pixel_text(out,text,kx+inset*.5f,ky+inset*.5f,kw-inset*1.5f,kh-inset*1.5f,z,ink);
            if(end==std::string::npos)break;start=end+1;
        }
    }
    else if(family==Family::Keyboard) {
        const float lip=std::max(.7f,std::min(w,h)*.09f),border=std::max(.45f,std::min(w,h)*.045f);
        rect(out,x+lip,y+lip,w-lip,h-lip,z,alpha|0x00151a1du);
        rect(out,x,y,w-lip,h-lip,z,alpha|0x00848e94u);
        rect(out,x+border,y+border,w-lip-border*2,h-lip-border*2,z,alpha|0x00f1f3f2u);
        rect(out,x+border,y+border,w-lip-border*2,border*.7f,z,alpha|0x00ffffffu);
        pixel_text(out,label,x+border*2,y+border*2,w-lip-border*4,h-lip-border*4,z,alpha|0x00212a2fu);
    } else if((family==Family::PlayStation) && (label=="Cross" || label=="Circle" || label=="Square" || label=="Triangle")) {
        const float border=std::max(.5f,std::min(w,h)*.06f);
        disc(out,cx,cy,r*1.18f,z,alpha|0x00272b31u,true,border);
        disc(out,cx,cy,r*1.18f,z,alpha|0x00c8cbd0u,false,border);
        const auto symbol=label=="Cross" ? alpha|0x005cb2eeu : label=="Circle" ? alpha|0x00ee5c69u : label=="Square" ? alpha|0x00dd70b9u : alpha|0x0078d087u;
        ps_symbol(out,label,cx,cy,r*.8f,z,symbol,border);
    } else if((family==Family::Nintendo || family==Family::Xbox) && label.size()==1 && std::string_view("ABXY").find(label[0])!=std::string_view::npos) {
        const float border=std::max(.45f,std::min(w,h)*.055f);
        const auto face=family==Family::Nintendo ? alpha|0x003c444au : label=="A" ? alpha|0x0046b85eu : label=="B" ? alpha|0x00d94143u : label=="X" ? alpha|0x00448bd8u : alpha|0x00dabc3du;
        const auto text=family==Family::Xbox ? alpha|0x00ffffffu : alpha|0x00f0f2f3u;
        disc(out,cx,cy,r*1.18f,z,alpha|0x00181c20u,true,border);
        disc(out,cx,cy,r*1.18f,z,alpha|0x007b858bu,false,border);
        disc(out,cx,cy,r*.9f,z,face,true,border);
        pixel_text(out,label,cx-r*.48f,cy-r*.42f,r*.96f,r*.84f,z,text);
    } else {
        const float border=std::max(.5f,std::min(w,h)*.06f);
        const auto body=family==Family::PlayStation ? alpha|0x00d7dadeu : family==Family::Nintendo ? alpha|0x003c444au : family==Family::Xbox ? alpha|0x0031393eu : alpha|0x00202d20u;
        const auto outline=family==Family::PlayStation ? alpha|0x00ffffffu : family==Family::Nintendo ? alpha|0x00848d92u : family==Family::Xbox ? alpha|0x007d898fu : ink;
        rect(out,x+inset,y+inset,w,h,z,alpha|0x0013181bu);
        rect(out,x,y,w-inset,h-inset,z,body);
        rect(out,x,y,border,h-inset,z,outline);rect(out,x,y,w-inset,border,z,outline);
        pixel_text(out,label,x+border*1.5f,y+border*1.5f,w-inset-border*3,h-inset-border*3,z,
            family==Family::PlayStation ? alpha|0x0032373du : alpha|0x00f1f3f4u);
    }
    return out;
}
}
bool replace_native_draw(const gpu::DrawCall &call,unsigned sheet,const std::function<std::string(input::Action)> &lookup,std::vector<gpu::DrawCall> &out,Family family) {
    auto kept=call;kept.vertices.clear();bool changed=false;
    for(std::size_t i=0;i+1<call.vertices.size();i+=2){const auto &a=call.vertices[i],&b=call.vertices[i+1];
        const auto action=native_action(sheet,a.texcoord[0],a.texcoord[1],b.texcoord[0],b.texcoord[1]);
        if(!action){kept.vertices.push_back(a);kept.vertices.push_back(b);continue;}
        const float w=std::abs(a.position[0]-b.position[0]),h=std::abs(a.position[1]-b.position[1]);
        if(w<4 || h<4 || w>64 || h>64){kept.vertices.push_back(a);kept.vertices.push_back(b);continue;}
        if(!kept.vertices.empty()){out.push_back(std::move(kept));kept=call;kept.vertices.clear();}
        const auto label=lookup(*action);out.push_back(cap(call,a,b,label,family));changed=true;
        if(std::getenv("MHFU_NATIVE_PROMPTS_TRACE")) {
            static std::set<std::string> seen;const auto key=std::to_string(sheet)+":"+std::to_string(static_cast<int>(*action))+":"+label;
            if(seen.size()>256)seen.clear();if(seen.insert(key).second){std::printf("[native-prompts] action=%u label=%s uv=%.1f,%.1f..%.1f,%.1f xy=%.1f,%.1f..%.1f,%.1f\n",unsigned(*action),label.c_str(),a.texcoord[0],a.texcoord[1],b.texcoord[0],b.texcoord[1],a.position[0],a.position[1],b.position[0],b.position[1]);std::fflush(stdout);}
        }
    }
    if(changed && !kept.vertices.empty())out.push_back(std::move(kept));
    return changed;
}
}
