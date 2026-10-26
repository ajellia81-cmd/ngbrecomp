#include "gpu/scene_motion.h"
#ifdef NGB_SMOOTH_MOTION
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>

namespace gpu {
namespace {
using V=ScenePosition;
using M=video::Mat4;
using Clock=std::chrono::steady_clock;
struct Token {
    uint32_t t[4];
    unsigned f(int w,int b,int n) const { return (t[w]>>b)&((1u<<n)-1); }
};
struct Operand { unsigned mux,reg,constant; bool neg,relative; unsigned sw[4]; };
Operand operand(const Token& k,int which) {
    Operand a{}; a.constant=k.f(1,13,8); a.relative=k.f(3,1,1);
    if(which==0) {
        a.mux=k.f(2,26,2);a.reg=k.f(2,28,4);a.neg=k.f(1,8,1);
        for(int i=0;i<4;i++) a.sw[i]=k.f(1,6-2*i,2);
    } else if(which==1) {
        a.mux=k.f(2,11,2);a.reg=k.f(2,13,4);a.neg=k.f(2,25,1);
        for(int i=0;i<4;i++) a.sw[i]=k.f(2,23-2*i,2);
    } else {
        a.mux=k.f(3,28,2);a.reg=(k.f(2,0,2)<<2)|k.f(3,30,2);a.neg=k.f(2,10,1);
        for(int i=0;i<4;i++) a.sw[i]=k.f(2,8-2*i,2);
        unsigned ilu=k.f(1,25,3);
        if(ilu>=2 && ilu<=6) for(int i=1;i<4;i++) a.sw[i]=a.sw[0];
    }
    if(a.mux==2) a.reg=k.f(1,9,4);
    return a;
}
struct Operation {
    Operand a,b,c;
    unsigned mac=0,ilu=0,r=0,mm=0,im=0,om=0;
    bool output_ilu=false,arl=false;
};
// Slice backwards from oPos. Colour/UV/lighting calculations must not turn
// this geometry conversion into a second full software renderer.
struct Program {
    std::vector<Operation> ops;
    std::array<uint8_t,192> constants{};
    uint16_t inputs=0;
    bool valid=true;
    explicit Program(const std::vector<uint32_t>& words) {
        std::vector<Token> tokens;
        for(size_t n=0;n+4<=words.size();n+=4) {
            Token k{};std::memcpy(k.t,&words[n],16);tokens.push_back(k);
            if(k.f(3,0,1)) break;
        }
        unsigned live[13]{};live[12]=15;bool address=false;
        auto need=[&](const Operand& o,unsigned mask) {
            if(!mask) return;
            unsigned source=0;
            for(unsigned i=0;i<4;i++) if(mask&(8u>>i)) source|=8u>>o.sw[i];
            if(o.mux==1) { if(o.reg<=12) live[o.reg]|=source; }
            else if(o.mux==2) inputs|=uint16_t(1u<<o.reg);
            else if(o.relative) {constants.fill(1);address=true;}
            else if(o.constant<192) constants[o.constant]=1;
            else valid=false;
        };
        for(auto it=tokens.rbegin();it!=tokens.rend();++it) {
            const Token& k=*it;Operation o{};
            o.mac=k.f(1,21,4);o.ilu=k.f(1,25,3);o.r=k.f(3,20,4);
            o.a=operand(k,0);o.b=operand(k,1);o.c=operand(k,2);
            o.mm=o.mac && o.mac!=13 && !(o.ilu && o.r==1) && o.r<=12 ? k.f(3,24,4):0;
            unsigned ir=o.mac?1:o.r;
            o.im=o.ilu && ir<=12?k.f(3,16,4):0;
            o.output_ilu=k.f(3,2,1);
            o.om=k.f(3,11,1) && (k.f(3,3,8)&15)==0 &&
                 ((o.output_ilu && o.ilu)||(!o.output_ilu && o.mac && o.mac!=13))?k.f(3,12,4):0;
            unsigned mr=0,iv=0;
            // Reverse the actual parallel instruction's write order. Operands
            // all read the registers before any of these writes.
            unsigned out=live[12]&o.om;live[12]&=~o.om;
            (o.output_ilu?iv:mr)|=out;
            if(o.im) {iv|=live[ir]&o.im;live[ir]&=~o.im;}
            if(o.mm) {mr|=live[o.r]&o.mm;live[o.r]&=~o.mm;}
            o.arl=o.mac==13 && address;
            if(o.mac==13) {address=false;if(o.arl) mr|=8;}
            if(!(mr||iv||o.arl)) continue;
            if(mr) {
                switch(o.mac) {
                case 1:case 13:need(o.a,mr);break;
                case 2:case 9:case 10:case 11:case 12:need(o.a,mr);need(o.b,mr);break;
                case 3:need(o.a,mr);need(o.c,mr);break;
                case 4:need(o.a,mr);need(o.b,mr);need(o.c,mr);break;
                case 5:need(o.a,14);need(o.b,14);break;
                case 6:need(o.a,14);need(o.b,15);break;
                case 7:need(o.a,15);need(o.b,15);break;
                case 8:need(o.a,mr&6);need(o.b,mr&5);break;
                default:valid=false;break;
                }
            } else {o.mac=0;o.mm=0;}
            if(iv) need(o.c,o.ilu==1?iv:o.ilu<=6?8:15);
            else {o.ilu=0;o.im=0;}
            // Keep the original choice of the ILU destination even when its
            // paired MAC is dead in this slice.
            if(ir==1 && o.ilu) o.im|=0x100;
            ops.push_back(o);
        }
        std::reverse(ops.begin(),ops.end());
        if(ops.empty()) valid=false;
    }
};
float clamp_magnitude(float x) {
    const float m=std::clamp(std::fabs(x),5.42101e-20f,1.84467e19f);return x<0?-m:m;
}
float product(float a,float b) {return a==0 || b==0 ? 0 : a*b;}
bool read_input(const Assembled& a,unsigned reg,uint32_t vertex,V& out) {
    out={0,0,0,1};
    if(reg==3) out={1,1,1,1};
    if(!(a.regs&(1u<<reg))) return true;
    const auto& e=a.elements[reg];if(e.buffer>=a.buffer_count) return false;
    const auto& b=a.buffers[e.buffer];size_t offset=size_t(vertex)*b.stride+e.offset;
    unsigned n=0,bytes=0,kind=0;
    switch(e.format) {
    case 2:n=4;bytes=16;break;case 6:n=3;bytes=12;break;case 16:n=2;bytes=8;break;case 41:n=1;bytes=4;break;
    case 13:n=4;bytes=8;kind=1;break;case 37:n=2;bytes=4;kind=1;break;case 58:n=1;bytes=2;kind=1;break;
    case 14:n=4;bytes=8;kind=2;break;case 38:n=2;bytes=4;kind=2;break;case 59:n=1;bytes=2;kind=2;break;
    case 28:n=4;bytes=4;kind=3;break;case 49:n=2;bytes=2;kind=3;break;case 61:n=1;bytes=1;kind=3;break;
    case 87:n=4;bytes=4;kind=4;break;case 42:n=1;bytes=4;kind=5;break;
    default:return false;
    }
    if(!b.src || offset>b.bytes || bytes>b.bytes-offset) return false;
    const uint8_t* p=b.src+offset;out={0,0,0,1};
    for(unsigned i=0;i<n;i++) {
        if(kind==0) std::memcpy(&out[i],p+i*4,4);
        else if(kind<=2) {int16_t v;std::memcpy(&v,p+i*2,2);out[i]=kind==1?std::max(float(v)/32767,-1.f):float(v);}
        else if(kind<=4) out[i]=p[kind==4 && i<3 ? 2-i:i]/255.f;
    }
    auto decode=unsigned(a.decode>>(reg*4))&15;
    if(kind==5) {
        uint32_t u;std::memcpy(&u,p,4);
        if(decode!=kDecodePacked) return false;
        out={std::max(float(int32_t(u<<21)>>21)/1023,-1.f),std::max(float(int32_t(u<<10)>>21)/1023,-1.f),
            std::max(float(int32_t(u)>>22)/511,-1.f),1};
    }
    if(decode==kDecodeW1 || decode==kDecodeIntW1) out[3]=1;
    if(decode==kDecodeFloat2H) {out[3]=out[2];out[2]=0;}
    return true;
}
V transform_point(const M& m,const V& p) {
    V out{};for(int r=0;r<4;r++) for(int c=0;c<4;c++) out[r]+=m.m[c*4+r]*p[c];return out;
}
M constant_matrix(const float* p) {
    M m{};for(int r=0;r<4;r++) for(int c=0;c<4;c++) m.m[c*4+r]=p[r*4+c];return m;
}
bool inverse(const M& in,M& out) {
    double a[4][8]{};
    for(int r=0;r<4;r++) for(int c=0;c<4;c++) {a[r][c]=in.m[c*4+r];a[r][c+4]=r==c;}
    for(int c=0;c<4;c++) {
        int best=c;for(int r=c+1;r<4;r++) if(std::fabs(a[r][c])>std::fabs(a[best][c])) best=r;
        if(!std::isfinite(a[best][c]) || std::fabs(a[best][c])<1e-14) return false;
        for(int j=0;j<8;j++) std::swap(a[c][j],a[best][j]);
        double d=a[c][c];for(double &v:a[c]) v/=d;
        for(int r=0;r<4;r++) if(r!=c) {double f=a[r][c];for(int j=0;j<8;j++) a[r][j]-=f*a[c][j];}
    }
    for(int r=0;r<4;r++) for(int c=0;c<4;c++) {out.m[c*4+r]=float(a[r][c+4]);if(!std::isfinite(out.m[c*4+r])) return false;}
    return true;
}
bool affine(const M& m) {
    for(float f:m.m) if(!std::isfinite(f)) return false;
    // A roundoff check on a mathematical invariant, not a motion threshold.
    return std::fabs(m.m[3])<2e-4f && std::fabs(m.m[7])<2e-4f && std::fabs(m.m[11])<2e-4f && std::fabs(m.m[15]-1)<2e-4f;
}
bool execute(const Program& p,const State& s,const Assembled& a,uint32_t vertex,V& pos) {
    if(!p.valid) return false;
    V inputs[16]{},r[13]{};int address=0;
    for(unsigned i=0;i<16;i++) if(p.inputs&(1u<<i)) if(!read_input(a,i,vertex,inputs[i])) return false;
    auto source=[&](const Operand& o) {
        V base{};
        if(o.mux==1) {if(o.reg<=12) base=r[o.reg];}
        else if(o.mux==2) base=inputs[o.reg];
        else {int index=o.relative?std::clamp(address+int(o.constant),0,191):int(o.constant);if(index<192) base=s.vsc[size_t(index)];}
        V v{};for(int i=0;i<4;i++) v[i]=(o.neg?-1.f:1.f)*base[o.sw[i]];return v;
    };
    auto write=[](V& dest,const V& value,unsigned mask) {for(unsigned i=0;i<4;i++) if(mask&(8u>>i)) dest[i]=value[i];};
    for(const auto& o:p.ops) {
        V aa=source(o.a),bb=source(o.b),cc=source(o.c),mr{},ir{};
        float dot=0;
        if(o.mac>=5 && o.mac<=7) {
            for(int i=0;i<(o.mac==7?4:3);i++) dot+=aa[i]*bb[i];
            if(o.mac==6) dot+=bb[3];
        }
        for(int i=0;i<4;i++) switch(o.mac) {
        case 1:case 13:mr[i]=aa[i];break;
        case 2:mr[i]=product(aa[i],bb[i]);break;
        case 3:mr[i]=aa[i]+cc[i];break;
        case 4:mr[i]=product(aa[i],bb[i])+cc[i];break;
        case 5:case 6:case 7:mr[i]=dot;break;
        case 8:mr[i]=i==0?1:i==1?aa[1]*bb[1]:i==2?aa[2]:bb[3];break;
        case 9:mr[i]=std::fmin(aa[i],bb[i]);break;case 10:mr[i]=std::fmax(aa[i],bb[i]);break;
        case 11:mr[i]=aa[i]<bb[i];break;case 12:mr[i]=aa[i]>=bb[i];break;
        default:break;
        }
        switch(o.ilu) {
        case 1:ir=cc;break;
        case 2:ir.fill(1.f/cc[0]);break;
        case 3:ir.fill(clamp_magnitude(1.f/cc[0]));break;
        case 4:ir.fill(1.f/std::sqrt(std::fabs(cc[0])));break;
        case 5: {float whole=std::floor(cc[0]);ir={std::exp2(whole),cc[0]-whole,std::exp2(cc[0]),1};break;}
        case 6: {float mag=std::fabs(cc[0]),l=std::log2(mag),e=std::floor(l);ir={e,mag==0?1:mag/std::exp2(e),l,1};break;}
        case 7: {float diffuse=std::fmax(cc[0],0),spec=std::exp2(std::clamp(cc[3],-127.99609375f,127.99609375f)*std::log2(std::fmax(cc[1],0)));ir={1,diffuse,diffuse>0?spec:0,1};break;}
        default:break;
        }
        if(o.arl) {if(!std::isfinite(mr[0]) || std::fabs(mr[0])>1000000) return false;address=int(std::floor(mr[0]+1e-3f));}
        if(o.mm) write(r[o.r],mr,o.mm);
        if(o.im&15) write(r[o.im&0x100?1:o.r],ir,o.im&15);
        if(o.om) write(r[12],o.output_ilu?ir:mr,o.om);
    }
    pos=r[12];for(float f:pos) if(!std::isfinite(f)) return false;return true;
}
bool fixed_position(const State& s,const Assembled& a,uint32_t vertex,const DrawConstants& c,V& pos) {
    V p;if(!read_input(a,0,vertex,p)) return false;p[3]=1;
    auto f=MakeFixedKey(s);V clip{};
    if(!f.blend) clip=transform_point(constant_matrix(c.wvp),p);
    else {
        V weights;if(!read_input(a,1,vertex,weights)) return false;
        bool mix=(f.blend&1)!=0;int n=mix?int(f.blend+3)/2:int(f.blend/2+1);float remaining=1;V view{};
        for(int b=0;b<n;b++) {float w=mix && b==n-1?remaining:weights[b];remaining-=w;
            V v=transform_point(constant_matrix(c.wvb[b]),p);for(int r=0;r<3;r++) view[r]+=v[r]*w;}
        view[3]=1;clip=transform_point(constant_matrix(c.pv),view);
    }
    float w=clamp_magnitude(clip[3]);pos={clip[0]/w,clip[1]/w,clip[2]/w,w};
    for(float v:pos) if(!std::isfinite(v)) return false;
    return true;
}
std::vector<uint32_t> triangles(const Assembled& a) {
    uint32_t n=a.index_count?a.index_count:a.draw_count;
    std::vector<uint32_t> ix;ix.reserve(n);
    for(uint32_t i=0;i<n;i++) {
        int64_t id=i;
        if(a.index_count) {
            if(a.index16) {uint16_t t;std::memcpy(&t,static_cast<const uint8_t*>(a.index_src)+i*2,2);id=int64_t(t)+a.base_vertex;}
            else {uint32_t t;std::memcpy(&t,static_cast<const uint8_t*>(a.index_src)+i*4,4);id=int64_t(t)+a.base_vertex;}
        }
        if(id<0 || id>=a.vertex_count) return {};ix.push_back(uint32_t(id));
    }
    if(!a.strip) {ix.resize(ix.size()/3*3);return ix;}
    std::vector<uint32_t> result;result.reserve(ix.size()*3);
    for(size_t i=0;i+2<ix.size();i++) {
        uint32_t x=ix[i+(i&1)],y=ix[i+1-(i&1)],z=ix[i+2];
        if(x!=y && x!=z && y!=z) result.insert(result.end(),{x,y,z});
    }
    return result;
}
uint64_t digest(const void* data,size_t size,uint64_t h=1469598103934665603ull) {
    auto p=static_cast<const uint8_t*>(data);for(size_t i=0;i<size;i++) {h^=p[i];h*=1099511628211ull;}return h;
}
struct ViewKey {
    uint32_t format,size,x,y,w,h;
    bool operator<(const ViewKey& b) const {return std::tie(format,size,x,y,w,h)<std::tie(b.format,b.size,b.x,b.y,b.w,b.h);}
};
struct Binding {
    size_t viewport,model;
    std::shared_ptr<std::vector<uint32_t>> indices;
    uint32_t count;
    M projection;
    V scale,offset;
    SceneOutput* output;
    bool programmable;
};
} // namespace
struct SceneMotion::Impl {
    video::Scene before,now;
    uint64_t serial=0;
    bool resolved=false;
    Stats stats{};
    std::vector<Binding> bindings;
    std::map<ViewKey,size_t> viewports;
    std::unordered_map<uint64_t,std::shared_ptr<Program>> programs;
    std::unordered_map<uint64_t,uint32_t> resources;
    std::unordered_map<uint64_t,std::vector<std::pair<size_t,size_t>>> shared;
    uint32_t next_resource=1;
    double elapsed=0;
    size_t geometry_bytes=0;
};
SceneMotion::SceneMotion():impl_(std::make_unique<Impl>()) {}
SceneMotion::~SceneMotion()=default;
bool SceneVertexPosition(const State& s,const Assembled& a,uint32_t v,const DrawConstants& c,V& result) {
    if((s.vs_handle&1) && s.vs && !s.vs->program.empty()) return execute(Program(s.vs->program),s,a,v,result);
    return fixed_position(s,a,v,c,result);
}
bool SceneMotion::Add(const State& s,const Assembled& a,const DrawConstants& dc,SceneOutput& output) {
    auto& p=*impl_;auto begin=Clock::now();p.stats.draws++;
    auto done=[&](bool ok) {p.elapsed+=std::chrono::duration<double,std::milli>(Clock::now()-begin).count();if(!ok) p.stats.unsupported++;return ok;};
    if(a.pretransformed || a.topology!=Topology::Triangles || !a.vertex_count || a.vertex_count>262144 || !s.scene_camera_valid) return done(false);
    // This adapter currently consumes the main camera's scene; shadow maps and
    // offscreen effects keep their original recipe, rather than borrowing its camera.
    if(s.viewport.w<600 || s.viewport.h<400) return done(false);
    M projection{},view{};std::memcpy(projection.m,s.scene_projection.data(),64);std::memcpy(view.m,s.scene_view.data(),64);
    M inv_projection{},inv_model{};if(!inverse(projection,inv_projection) || !affine(view)) return done(false);
    bool program=(s.vs_handle&1) && s.vs && !s.vs->program.empty();
    std::shared_ptr<Program> vp;
    if(program) {
        uint64_t h=digest(s.vs->program.data(),s.vs->program.size()*4);
        auto& slot=p.programs[h];if(!slot) slot=std::make_shared<Program>(s.vs->program);vp=slot;
        if(!vp->valid) return done(false);
    }
    M model=view;
    bool use_center=false;
    if(program && vp->constants[96] && vp->constants[97] && vp->constants[98] && vp->constants[99]) {
        M candidate=inv_projection*constant_matrix(s.vsc[96].data());
        if(affine(candidate)) model=candidate;else {p.stats.fallback++;use_center=true;}
    } else if(!program && !MakeFixedKey(s).blend) model=constant_matrix(dc.wv);
    else {p.stats.fallback++;use_center=true;}
    if(!inverse(model,inv_model)) return done(false);
    if(!program) {
        // Fixed function already outputs Vulkan clip coordinates, with the
        // viewport/depth mapping and half-pixel correction in this matrix.
        projection=MakeFixedKey(s).blend?constant_matrix(dc.pv):constant_matrix(dc.wvp)*inv_model;
        if(!inverse(projection,inv_projection)) return done(false);
    }
    M local_from_clip=inv_model*inv_projection;
    auto ix=std::make_shared<std::vector<uint32_t>>(triangles(a));if(ix->empty()) return done(false);
    size_t owned=ix->size()*(sizeof(video::Vertex)+sizeof(uint32_t))+size_t(a.vertex_count)*sizeof(V);
    if(owned>64u*1024u*1024u-p.geometry_bytes) return done(false);
    p.geometry_bytes+=owned;
    V scale{},offset{};
    if(program) {scale=s.vsc[58];offset=s.vsc[59];}
    else {scale={1,1,1,0};offset={0,0,0,0};}
    for(int i=0;i<3;i++) if(!std::isfinite(scale[i]) || std::fabs(scale[i])<1e-12f || !std::isfinite(offset[i])) return done(false);
    auto meshes=std::make_shared<std::vector<video::Mesh>>(1);auto& mesh=(*meshes)[0];mesh.verts.resize(ix->size());
    std::vector<V> local(a.vertex_count);std::vector<uint8_t> seen(a.vertex_count);
    uint64_t resource=1469598103934665603ull;
    for(size_t t=0;t<ix->size();t++) {
        uint32_t id=(*ix)[t];
        if(!seen[id]) {
            V screen{};if(!(program?execute(*vp,s,a,id,screen):fixed_position(s,a,id,dc,screen))) return done(false);
            float w=clamp_magnitude(screen[3]);V clip{};
            for(int i=0;i<3;i++) clip[i]=(screen[i]-offset[i])/scale[i]*w;
            clip[3]=w;
            local[id]=transform_point(local_from_clip,clip);
            if(!std::isfinite(local[id][3]) || std::fabs(local[id][3])<1e-10f) return done(false);
            for(int i=0;i<3;i++) {local[id][i]/=local[id][3];if(!std::isfinite(local[id][i])) return done(false);}
            local[id][3]=1;seen[id]=1;
            V raw;if(!read_input(a,0,id,raw)) return done(false);resource=digest(raw.data(),sizeof raw,resource);
        }
        auto& vertex=mesh.verts[t];std::memcpy(vertex.pos,local[id].data(),12);
        V uv;if(!read_input(a,9,id,uv)) return done(false);vertex.uv[0]=uv[0];vertex.uv[1]=uv[1];
    }
    if(use_center) {
        float lo[3]={INFINITY,INFINITY,INFINITY},hi[3]={-INFINITY,-INFINITY,-INFINITY};
        for(const auto& vertex:mesh.verts) for(int r=0;r<3;r++) {lo[r]=std::min(lo[r],vertex.pos[r]);hi[r]=std::max(hi[r],vertex.pos[r]);}
        M center=M::identity();
        for(int r=0;r<3;r++) center.m[12+r]=(lo[r]+hi[r])*.5f;
        for(auto& vertex:mesh.verts) for(int r=0;r<3;r++) vertex.pos[r]-=center.m[12+r];
        model=model*center;
    }
    resource=digest(ix->data(),ix->size()*4,resource);
    mesh.key=ix->size(); // topology compatibility; texture/UV output stays in the original shader
    auto found=p.resources.find(resource);
    if(found==p.resources.end()) found=p.resources.emplace(resource,p.next_resource++).first;
    ViewKey key{s.rt.format,s.rt.size,s.viewport.x,s.viewport.y,s.viewport.w,s.viewport.h};
    auto [where,inserted]=p.viewports.emplace(key,p.now.viewports.size());
    if(inserted) {
        video::Viewport viewport;viewport.vp_x=float(s.viewport.x);viewport.vp_y=float(s.viewport.y);viewport.vp_w=float(s.viewport.w);viewport.vp_h=float(s.viewport.h);
        viewport.root=view;viewport.root_matrix=0;
        // VF3 projects against a -Z view. Convert the Xbox +Z convention once
        // at the boundary, for both model matrices and the projection below.
        for(int col=0;col<4;col++) viewport.root.m[col*4+2]*=-1;
        viewport.left=-1/std::max(std::fabs(projection.m[0]),1e-12f);viewport.right=-viewport.left;
        viewport.bottom=-1/std::max(std::fabs(projection.m[5]),1e-12f);viewport.top=-viewport.bottom;
        p.now.viewports.push_back(std::move(viewport));
    }
    size_t v=where->second;
    for(int col=0;col<4;col++) model.m[col*4+2]*=-1;
    uint64_t same=digest(model.m,64,digest(&found->second,4));
    for(const auto& vertex:mesh.verts) same=digest(vertex.pos,sizeof vertex.pos,same);
    size_t index=p.now.viewports[v].models.size();bool reused=false;
    for(auto [u,j]:p.shared[same]) if(u==v) {
        const auto& previous=p.now.viewports[u].models[j];
        bool same_positions=previous.meshes->at(0).verts.size()==mesh.verts.size();
        if(same_positions) for(size_t i=0;i<mesh.verts.size();i++)
            if(std::memcmp(previous.meshes->at(0).verts[i].pos,mesh.verts[i].pos,sizeof mesh.verts[i].pos)) {same_positions=false;break;}
        if(previous.addr==found->second && !std::memcmp(previous.matrix.m,model.m,64) && same_positions) {index=j;reused=true;break;}
    }
    if(reused) p.stats.shared++;
    else {video::Model m;m.addr=found->second;m.matrix=model;m.meshes=meshes;p.now.viewports[v].models.push_back(std::move(m));p.shared[same].push_back({v,index});p.stats.models++;}
    p.bindings.push_back({v,index,ix,a.vertex_count,projection,scale,offset,&output,program});
    return done(true);
}
SceneMotion::Stats SceneMotion::Resolve() {
    auto& p=*impl_;if(p.resolved) return p.stats;p.resolved=true;
    auto start=Clock::now();p.now.id=++p.serial;
    // Canonical viewport order is independent of this frame's draw order.
    std::vector<video::Viewport> ordered;ordered.reserve(p.now.viewports.size());
    std::vector<size_t> remap(p.now.viewports.size());
    for(auto [key,old]:p.viewports) {remap[old]=ordered.size();ordered.push_back(std::move(p.now.viewports[old]));}
    p.now.viewports=std::move(ordered);
    for(auto& binding:p.bindings) binding.viewport=remap[binding.viewport];
    video::Scene output;
    if(!p.before.id || !video::interpolate(p.before,p.now,.5f,output,&p.stats.vf)) {p.stats.cuts++;p.stats.cpu_ms=p.elapsed;return p.stats;}
    for(const Binding& b:p.bindings) {
        const auto& m=output.viewports[b.viewport].models[b.model];
        const auto& original=p.now.viewports[b.viewport].models[b.model];
        if(!std::memcmp(m.matrix.m,original.matrix.m,64) && m.meshes==original.meshes) continue;
        std::vector<V> screen(b.count);std::vector<uint8_t> seen(b.count);
        const auto& vertices=m.meshes->at(0).verts;
        for(size_t i=0;i<vertices.size();i++) {
            uint32_t id=(*b.indices)[i];if(seen[id]) continue;seen[id]=1;
            V local{vertices[i].pos[0],vertices[i].pos[1],vertices[i].pos[2],1};
            V view=transform_point(m.matrix,local);view[2]*=-1;
            V clip=transform_point(b.projection,view);float w=clamp_magnitude(clip[3]);
            if(b.programmable) {
                for(int r=0;r<3;r++) screen[id][r]=clip[r]/w*b.scale[r]+b.offset[r];
                screen[id][3]=w;
            } else screen[id]=clip;
        }
        b.output->positions=std::move(screen);p.stats.changed++;
    }
    p.stats.cpu_ms=p.elapsed+std::chrono::duration<double,std::milli>(Clock::now()-start).count();return p.stats;
}
void SceneMotion::Finish() {
    auto& p=*impl_;if(!p.resolved) Resolve();p.before=std::move(p.now);p.now={};p.bindings.clear();p.viewports.clear();p.shared.clear();p.stats={};p.elapsed=0;p.geometry_bytes=0;p.resolved=false;
    // Resource identities only index owned geometry; no guest/GPU pointer survives.
    if(p.resources.size()>131072) {p.resources.clear();p.next_resource=1;p.before={};}
}
void SceneMotion::Reset() {
    auto& p=*impl_;p.before={};p.now={};p.bindings.clear();p.viewports.clear();p.shared.clear();p.stats={};p.elapsed=0;p.geometry_bytes=0;p.resolved=false;
}
} // namespace gpu
#endif
