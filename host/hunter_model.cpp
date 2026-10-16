#include "hunter_model.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <span>

namespace portablekit::fu::model {
namespace {
constexpr std::uint32_t legacy_magic=0x314d5546,magic=0x324d5546; // FUM2; local derived assets
constexpr std::uintmax_t limit=64*1024*1024;
template<class T> bool scalar(std::istream &s,T &v) { return bool(s.read(reinterpret_cast<char *>(&v),sizeof(v))); }
template<class T> void scalar(std::ostream &s,const T &v) { s.write(reinterpret_cast<const char *>(&v),sizeof(v)); }
bool finite(const Vertex &v) {
    return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.nx)&&std::isfinite(v.ny)&&std::isfinite(v.nz)&&std::isfinite(v.u)&&std::isfinite(v.v)
        &&std::abs(v.x)<1e6f&&std::abs(v.y)<1e6f&&std::abs(v.z)<1e6f&&std::abs(v.u)<1e6f&&std::abs(v.v)<1e6f;
}
}
bool write(const std::filesystem::path &path,const Scene &scene) {
    if(scene.meshes.empty()||scene.meshes.size()>512) return false;
    std::error_code ec; std::filesystem::create_directories(path.parent_path(),ec); if(ec) return false;
    auto temp=path; temp+=".tmp";
    std::ofstream out(temp,std::ios::binary|std::ios::trunc);
    scalar(out,magic); scalar(out,static_cast<std::uint32_t>(scene.meshes.size()));
    for(const auto &m:scene.meshes) {
        scalar(out,static_cast<std::uint32_t>(m.vertices.size())); scalar(out,m.width); scalar(out,m.height);
        scalar(out,m.wrap_u); scalar(out,m.wrap_v); scalar(out,m.texturing); scalar(out,m.alpha_reference);
        for(auto field:m.texture_state) scalar(out,field);
        scalar(out,static_cast<std::uint32_t>(m.texture_bytes.size())); scalar(out,static_cast<std::uint32_t>(m.palette_bytes.size()));
        out.write(reinterpret_cast<const char *>(m.vertices.data()),m.vertices.size()*sizeof(Vertex));
        out.write(reinterpret_cast<const char *>(m.pixels.data()),m.pixels.size()*sizeof(std::uint32_t));
        out.write(reinterpret_cast<const char *>(m.texture_bytes.data()),m.texture_bytes.size());
        out.write(reinterpret_cast<const char *>(m.palette_bytes.data()),m.palette_bytes.size());
    }
    out.flush(); const bool good=bool(out)&&out.tellp()<=limit; out.close();
    if(!good) { std::filesystem::remove(temp,ec); return false; }
    std::filesystem::rename(temp,path,ec); if(ec) { std::filesystem::remove(temp,ec); return false; } return true;
}
bool read(const std::filesystem::path &path,Scene &scene) {
    scene={}; std::error_code ec; const auto size=std::filesystem::file_size(path,ec);
    if(ec||size>limit) return false;
    std::ifstream in(path,std::ios::binary); std::uint32_t tag{},count{}; Scene next;
    if(!scalar(in,tag)||(tag!=magic&&tag!=legacy_magic)||!scalar(in,count)||!count||count>512) return false;
    std::size_t remaining=static_cast<std::size_t>(size>=8?size-8:0);
    for(unsigned i=0;i<count;++i) {
        Mesh m; std::uint32_t n{};
        if(!scalar(in,n)||!scalar(in,m.width)||!scalar(in,m.height)||!scalar(in,m.wrap_u)||!scalar(in,m.wrap_v)||!scalar(in,m.texturing)||!scalar(in,m.alpha_reference)) return false;
        std::uint32_t raw{},palette{};
        if(tag==magic) {
            for(auto &field:m.texture_state) if(!scalar(in,field)) return false;
            if(!scalar(in,raw)||!scalar(in,palette)||raw>16*1024*1024||palette>4096) return false;
        }
        if(!n||n%3||n>200000||m.width>2048||m.height>2048||((m.width==0)!=(m.height==0))||m.texturing>4||m.alpha_reference>255) return false;
        const std::size_t bytes=(tag==magic?100:28)+static_cast<std::size_t>(n)*sizeof(Vertex)+static_cast<std::size_t>(m.width)*m.height*4+raw+palette;
        if(bytes>remaining) return false; remaining-=bytes;
        m.vertices.resize(n); m.pixels.resize(static_cast<std::size_t>(m.width)*m.height);
        if(!in.read(reinterpret_cast<char *>(m.vertices.data()),m.vertices.size()*sizeof(Vertex))||!in.read(reinterpret_cast<char *>(m.pixels.data()),m.pixels.size()*4)) return false;
        m.texture_bytes.resize(raw);m.palette_bytes.resize(palette);
        if(!in.read(reinterpret_cast<char *>(m.texture_bytes.data()),raw)||!in.read(reinterpret_cast<char *>(m.palette_bytes.data()),palette)) return false;
        for(const auto &v:m.vertices) if(!finite(v)) return false;
        next.meshes.push_back(std::move(m));
    }
    if(remaining) return false;
    scene=std::move(next); return true;
}
std::vector<std::uint32_t> render(const Scene &scene,unsigned w,unsigned h,float yaw) {
    if(!w||!h||w>1024||h>1024||!std::isfinite(yaw)) return {};
    std::vector<std::uint32_t> image(static_cast<std::size_t>(w)*h,0);
    float lo[3]={1e30f,1e30f,1e30f},hi[3]={-1e30f,-1e30f,-1e30f};
    for(const auto &m:scene.meshes) for(const auto &v:m.vertices) {
        if(!finite(v)) return image;
        const float p[]={v.x,v.y,v.z}; for(int j=0;j<3;++j) {lo[j]=std::min(lo[j],p[j]);hi[j]=std::max(hi[j],p[j]);}
    }
    if(hi[1]-lo[1]<.001f) return image;
    const float cx=(lo[0]+hi[0])*.5f,cy=(lo[1]+hi[1])*.5f,cz=(lo[2]+hi[2])*.5f;
    // Use the horizontal bounding circle so turning never clips a weapon.
    const float diameter=std::hypot(hi[0]-lo[0],hi[2]-lo[2]);
    const float scale=std::min(w*.90f/std::max(diameter,.001f),h*.94f/(hi[1]-lo[1]));
    const float c=std::cos(yaw),s=std::sin(yaw);
    std::vector<float> depth(image.size(),std::numeric_limits<float>::infinity());
    struct Projected {float x,y,z,u,v,r,g,b,a;};
    const auto project=[&](const Vertex &v) {
        const float x=v.x-cx,z=v.z-cz;
        const float nx=c*v.nx+s*v.nz,ny=v.ny,nz=-s*v.nx+c*v.nz;
        const float length=std::sqrt(nx*nx+ny*ny+nz*nz);
        const float light=length>.0001f?.65f+.35f*std::max(0.f,(nx*-.35f+ny*.5f+nz*.79f)/length):1.f;
        return Projected{w*.5f+(c*x+s*z)*scale,h*.5f-(v.y-cy)*scale,-s*x+c*z,v.u,v.v,
            (v.color&255)*light,((v.color>>8)&255)*light,((v.color>>16)&255)*light,float(v.color>>24)};
    };
    const auto edge=[](const Projected &a,const Projected &b,float x,float y){return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);};
    for(const auto &m:scene.meshes) for(std::size_t i=0;i+2<m.vertices.size();i+=3) {
        const auto a=project(m.vertices[i]),b=project(m.vertices[i+1]),d=project(m.vertices[i+2]);
        const float area=edge(a,b,d.x,d.y); if(std::abs(area)<.00001f) continue;
        const int x0=std::max(0,int(std::floor(std::min({a.x,b.x,d.x})))),x1=std::min(int(w)-1,int(std::ceil(std::max({a.x,b.x,d.x}))));
        const int y0=std::max(0,int(std::floor(std::min({a.y,b.y,d.y})))),y1=std::min(int(h)-1,int(std::ceil(std::max({a.y,b.y,d.y}))));
        for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
            const float p=edge(b,d,x+.5f,y+.5f)/area,q=edge(d,a,x+.5f,y+.5f)/area,r=1-p-q;
            if(p<0||q<0||r<0) continue;
            const float z=p*a.z+q*b.z+r*d.z; const auto at=static_cast<std::size_t>(y)*w+x;
            // The original preview camera looks down -Z: greater camera Z is nearer.
            if(-z>=depth[at]) continue;
            float color[]={p*a.r+q*b.r+r*d.r,p*a.g+q*b.g+r*d.g,p*a.b+q*b.b+r*d.b,p*a.a+q*b.a+r*d.a};
            if(m.width&&m.height&&m.pixels.size()==static_cast<std::size_t>(m.width)*m.height) {
                const auto coord=[](float v,unsigned n,unsigned clamp) { v=clamp?std::clamp(v,0.f,1.f):v-std::floor(v); return std::min(n-1,static_cast<unsigned>(v*n)); };
                const auto tex=m.pixels[coord(p*a.v+q*b.v+r*d.v,m.height,m.wrap_v)*m.width+coord(p*a.u+q*b.u+r*d.u,m.width,m.wrap_u)];
                for(int j=0;j<4;++j) { const float t=float((tex>>(j*8))&255); color[j]=(m.texturing==3)?t:color[j]*t/255.f; }
            }
            if(color[3]<=m.alpha_reference||color[3]<1) continue;
            std::uint32_t pixel{}; for(int j=0;j<4;++j) pixel|=static_cast<std::uint32_t>(std::clamp(color[j],0.f,255.f))<<(j*8);
            image[at]=pixel; depth[at]=-z;
        }
    }
    return image;
}
}
