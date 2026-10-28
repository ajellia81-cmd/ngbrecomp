// Included in shaders.cpp, reusing the original opcode/math generators instead
// of maintaining a second definition of their arithmetic. No recorded recipes.
UniversalConstants MakeUniversalConstants(const State& s,const Assembled& a,const PixelKey& pixel,uint32_t flags) {
    static_assert(sizeof(PixelKey)==340 && sizeof(FixedKey)==28);
    UniversalConstants out;
    std::memcpy(out.words.data(),&pixel,sizeof pixel);
    out[uFlags]=flags;out[uRegisters]=a.regs;
    out[uDecodeLo]=uint32_t(a.decode);out[uDecodeHi]=uint32_t(a.decode>>32);
    auto fixed=MakeFixedKey(s);std::memcpy(&out.words[uFixed/4],&fixed,sizeof fixed);
    if((flags&uProgrammableVertex) && s.vs) {
        if(s.vs->program.size()>544) throw std::runtime_error("NV2A program exceeds universal instruction memory");
        out[uProgramWords]=uint32_t(s.vs->program.size());
        std::memcpy(&out.words[uProgram/4],s.vs->program.data(),s.vs->program.size()*4);
    }
    return out;
}
void UniversalVertexOffsets(UniversalConstants& out,const Assembled& a,const uint64_t *offsets) {
    for(unsigned r=0;r<16;r++) if(a.regs&(1u<<r)) {
        const auto& e=a.elements[r];
        if(e.buffer>=a.buffer_count)throw std::runtime_error("Invalid universal vertex buffer");
        uint64_t offset=offsets[e.buffer]+e.offset;
        if(offset>UINT32_MAX)throw std::runtime_error("Universal vertex offset exceeds storage address space");
        out[uVertex+r*4]=uint32_t(offset);out[uVertex+r*4+1]=a.buffers[e.buffer].stride;
        out[uVertex+r*4+2]=a.buffers[e.buffer].bytes;out[uVertex+r*4+3]=e.format;
    }
}
namespace {
std::string UniversalCommon(bool vertex) {
    std::string s=Common();size_t at=s.find("    float fogamt : TEXCOORD4;\n");
    s.insert(at,"    float4 ptc : TEXCOORD5;\n");
    if(vertex) {
        at=s.find("struct VOut {\n")+std::strlen("struct VOut {\n");
        s.insert(at,"    [[vk::builtin(\"PointSize\")]] float psize : PSIZE;\n");
    }
    s+=R"(
[[vk::binding(26,0)]] cbuffer UniversalData { uint4 ud[224]; };
uint U(uint n) { return ud[n>>2][n&3]; }
bool Flag(uint f) {return (U(85)&f)!=0;}
uint P(uint n) {return U(n);}
uint Fixed(uint n) {return U(96+n);}
bool HasVertex(uint r) {return (U(86)&(1u<<r))!=0;}
)";
    return s;
}
}

std::string UniversalVertexHlsl() {
    std::string s=UniversalCommon(true);s+=kVsHelpers;
    s+=R"(
[[vk::binding(27,0)]] ByteAddressBuffer vertex_data;
[[vk::binding(29,0)]] StructuredBuffer<float4> universal_scene_positions;
uint Read8(uint p) {return (vertex_data.Load(p&~3u)>>((p&3u)*8))&255u;}
uint Read16(uint p) {return (vertex_data.Load(p&~3u)>>((p&2u)*8))&65535u;}
float Signed16(uint p) {return float(int(Read16(p)<<16)>>16);}
float4 Vertex(uint r,uint id) {
    if(!HasVertex(r))return r==3 ? float4(1,1,1,1) : float4(0,0,0,1);
    uint base=112+r*4,ptr=U(base)+id*U(base+1),format=U(base+3);
    uint decode=((r<8?U(87):U(88))>>((r&7)*4))&15;
    if(decode==4) {
        uint raw=vertex_data.Load(ptr);
        int3 v=int3(int(raw<<21)>>21,int(raw<<10)>>21,int(raw)>>22);
        return float4(max(float3(v)/float3(1023,1023,511),-1),1);
    }
    float4 v=float4(0,0,0,1);
    if(format==2) v=asfloat(vertex_data.Load4(ptr));
    else if(format==6) v.xyz=asfloat(vertex_data.Load3(ptr));
    else if(format==16) v.xy=asfloat(vertex_data.Load2(ptr));
    else if(format==41) v.x=asfloat(vertex_data.Load(ptr));
    else if(format==42) v.x=float(vertex_data.Load(ptr));
    else if(format==13 || format==14 || format==37 || format==38 || format==58 || format==59) {
        uint n=(format==13 || format==14)?4:(format==37 || format==38)?2:1;
        bool normalized=format==13 || format==37 || format==58;
        for(uint lane=0;lane<n;lane++)v[lane]=normalized?max(Signed16(ptr+lane*2)/32767.,-1.):Signed16(ptr+lane*2);
    } else if(format==28 || format==87 || format==49 || format==61) {
        uint n=format==28 || format==87?4:format==49?2:1;
        for(uint lane=0;lane<n;lane++)v[lane]=Read8(ptr+lane)/255.;
        if(format==87)v=v.bgra;
    }
    else v=asfloat(vertex_data.Load4(ptr));
    if(decode==1 || decode==3)v.w=1;
    else if(decode==5)v=float4(v.xy,0,v.z);
    return v;
}
float4 Operand(uint4 k,uint which,uint id,int address,float4 temp[13]) {
    uint neg,sw,reg,mux;
    if(which==0) {neg=(k.y>>8)&1;sw=k.y&255;reg=(k.z>>28)&15;mux=(k.z>>26)&3;}
    else if(which==1) {neg=(k.z>>25)&1;sw=(k.z>>17)&255;reg=(k.z>>13)&15;mux=(k.z>>11)&3;}
    else {neg=(k.z>>10)&1;sw=(k.z>>2)&255;reg=((k.z&3)<<2)|(k.w>>30);mux=(k.w>>28)&3;}
    float4 v=0;
    if(mux==1) {if(reg<13)v=temp[reg];}
    else if(mux==2)v=Vertex((k.y>>9)&15,id);
    else {uint index=(k.y>>13)&255;if(k.w&2)index=uint(clamp(address+int(index),0,191));if(index<192)v=c[index];}
    uint4 lanes=uint4((sw>>6)&3,(sw>>4)&3,(sw>>2)&3,sw&3);
    uint ilu=(k.y>>25)&7;
    if(which==2 && ilu>=2 && ilu<=6)lanes=lanes.xxxx;
    return (neg!=0?-1.:1.)*float4(v[lanes.x],v[lanes.y],v[lanes.z],v[lanes.w]);
}
void MaskedWrite(inout float4 dest,float4 value,uint mask) {
    for(uint lane=0;lane<4;lane++)if(mask&(8u>>lane))dest[lane]=value[lane];
}
float4 Mac(uint op,float4 a,float4 b,float4 d) {switch(op) {
)";
    for(unsigned op=1;op<=13;op++)s+=F("case %u:return %s;\n",op,MacExpr(op,"a","b","d").c_str());
    s+="default:return 0;}}\nfloat4 Ilu(uint op,float4 d) {switch(op) {\n";
    for(unsigned op=1;op<=7;op++)s+=F("case %u:return %s;\n",op,IluExpr(op,"d").c_str());
    s+=R"(default:return 0;}}
float4 Material(uint field,float4 input_diffuse,float4 input_specular,float4 fallback) {
    uint source=(Fixed(0)>>(8+field*2))&3;
    return source==1 && HasVertex(3)?input_diffuse:source==2 && HasVertex(4)?input_specular:fallback;
}
VOut main(uint vertex_id:SV_VertexID) {
    precise float4 temp[13];for(uint r=0;r<13;r++)temp[r]=0;
    float4 outputs[16];for(uint r=0;r<16;r++)outputs[r]=float4(0,0,0,1);
    outputs[3]=outputs[7]=float4(0,0,0,1);outputs[4]=outputs[8]=outputs[5]=0;outputs[6]=1;
    float4 cp=0;float3 vp=0,vn=0;
    if(Flag(2)) {
        int address=0;
        for(uint pcounter=0;pcounter<min(U(89)/4,136u);pcounter++) {
            uint4 k=ud[48+pcounter];uint mac=(k.y>>21)&15,ilu=(k.y>>25)&7;
            if(mac!=0 || ilu!=0) {
                precise float4 a=Operand(k,0,vertex_id,address,temp),b=Operand(k,1,vertex_id,address,temp),d=Operand(k,2,vertex_id,address,temp);
                precise float4 mr=Mac(mac,a,b,d),ir=Ilu(ilu,d);
                uint reg=(k.w>>20)&15,mask=(k.w>>24)&15;
                if(mac==13)address=mac_arl(mr.x);
                else if(mac!=0 && !(ilu!=0 && reg==1) && reg<=12)MaskedWrite(temp[reg],mr,mask);
                uint ireg=mac!=0?1:reg;
                if(ilu!=0 && ireg<=12)MaskedWrite(temp[ireg],ir,(k.w>>16)&15);
                bool use_ilu=(k.w&4)!=0;uint omask=(k.w>>12)&15;
                if(omask!=0 && (k.w&2048)!=0 && (use_ilu?ilu!=0:mac!=0 && mac!=13)) {
                    uint outreg=(k.w>>3)&15;float4 value=use_ilu?ir:mr;
                    if(outreg==0)MaskedWrite(temp[12],value,omask);
                    else if(outreg==3 || outreg==4 || outreg==5 || outreg==6 || outreg==7 || outreg==8 || (outreg>=9 && outreg<=12)) {
                        if(outreg==5) {uint n=countbits(omask);for(uint lane=0;lane<n;lane++)outputs[5][lane]=value[lane];}
                        else MaskedWrite(outputs[outreg],value,omask);
                    }
                }
            }
            if(k.w&1)break;
        }
    } else {
        float4 ip=float4(Vertex(0,vertex_id).xyz,1);
        float4 input_normal=HasVertex(2)?float4(Vertex(2,vertex_id).xyz,0):0;
        uint blend=Fixed(6),light=Fixed(0);
        if(Flag(1)) {
            float4 rhw=Vertex(0,vertex_id);float w=rhw.w==0?1:1/rhw.w;
            temp[12]=float4(rhw.xy,rhw.z/surf.z,w);
        } else if(blend!=0) {
            uint matrices=(blend&1)?(blend+3)/2:blend/2+1;
            float4 weights=HasVertex(1)?Vertex(1,vertex_id):0;float last_weight=1;
            for(uint m=0;m<matrices;m++) {
                float weight=((blend&1)!=0 && m==matrices-1)?last_weight:weights[m];
                if((blend&1)!=0 && m<matrices-1)last_weight-=weights[m];
                vp+=mul(ip,wvb[m]).xyz*weight;vn+=mul(input_normal,wvbn[m]).xyz*weight;
            }
            cp=mul(float4(vp,1),pv);
        } else {cp=mul(ip,wvp);vp=mul(ip,wv).xyz;vn=mul(input_normal,wvn).xyz;}
        if(Flag(32) && !Flag(1)) {
            float4 opposite=mul(float4(Vertex(9,vertex_id).xyz,1),wvp);
            float4 stroke_data=Vertex(10,vertex_id);
            float2 delta=(opposite.xy/opposite.w-cp.xy/cp.w)*float2(surf.x,-surf.y)*.5;
            delta.x*=stroke_data.z;delta*=stroke_data.y;
            float2 normal=float2(-delta.y,delta.x)*rsqrt(max(dot(delta,delta),1e-20));
            normal.x/=stroke_data.z;
            cp.xy+=normal*(.5*stroke_data.w*stroke_data.x)*float2(2/surf.x,-2/surf.y)*cp.w;
        }
        if((light&8)!=0)vn=dot(vn,vn)>0?normalize(vn):vn;
        float4 diffuse=HasVertex(3)?Vertex(3,vertex_id):1,specular=HasVertex(4)?Vertex(4,vertex_id):0;
        if(!Flag(1) && (light&1)!=0) {
            float4 md=Material(0,diffuse,specular,mat[0]),ms=Material(1,diffuse,specular,mat[2]);
            float4 ma=Material(2,diffuse,specular,mat[1]),me=Material(3,diffuse,specular,mat[3]);
            float3 la=0,ld=0,ls=0,eye=(light&4)!=0?-normalize(vp):float3(0,0,-1);
            for(uint n=0;n<8;n++) {
                uint type=(Fixed(1)>>(n*2))&3;if(type==0)continue;uint at=n*7;
                float3 l;float att=1;
                if(type==3)l=-normalize(lt[at+4].xyz);
                else {
                    float3 d=lt[at+3].xyz-vp;float distance=length(d);l=d/max(distance,1e-6);
                    att=distance>lt[at+3].w?0:1/max(lt[at+5].x+lt[at+5].y*distance+lt[at+5].z*distance*distance,1e-6);
                    if(type==2) {float rho=dot(-l,normalize(lt[at+4].xyz));att*=rho>lt[at+6].x?1:rho<=lt[at+6].y?0:pow(saturate((rho-lt[at+6].y)/(lt[at+6].x-lt[at+6].y)),lt[at+4].w);}
                }
                la+=att*lt[at+2].rgb;float nl=dot(vn,l);ld+=att*lt[at].rgb*max(nl,0);
                if((light&2)!=0 && nl>0)ls+=att*lt[at+1].rgb*pow(max(dot(vn,normalize(l+eye)),0),mat[4].x);
            }
            outputs[3]=float4(me.rgb+ma.rgb*(amb.rgb+la)+md.rgb*ld,md.a);
            outputs[4]=(light&2)!=0?float4(ms.rgb*ls,ms.a):float4(0,0,0,0);
        } else {outputs[3]=diffuse;outputs[4]=specular;}
        outputs[7]=outputs[3];outputs[8]=outputs[4];
        for(uint t=0;t<4;t++) {
            uint state=Fixed(2+t),set=state&3,gen=(state>>4)&7,n=(state>>8)&7,size=(state>>12)&7;
            float4 input_tc=HasVertex(9+set)?Vertex(9+set,vertex_id):float4(0,0,0,1);
            if(Flag(1) || (gen==0 && n==0)) {outputs[9+t]=input_tc;continue;}
            float4 generated=gen==1?float4(vn,1):gen==2?float4(vp,1):gen==3?float4(reflect(normalize(vp),vn),1):
                size==1?float4(input_tc.x,1,0,0):size==2?float4(input_tc.xy,1,0):size==3?float4(input_tc.xyz,1):input_tc;
            float4 tc=mul(generated,tm[t]);
            outputs[9+t]=(state&2048)!=0?float4(tc.xyz,tc[min(n,4u)-1]):float4(tc.xy,n>=3?tc.z:0,1);
        }
        outputs[5]=HasVertex(5)?Vertex(5,vertex_id):0;
    }
    VOut o;
    if(Flag(2) || Flag(1)) {
        precise float4 p=temp[12];
)";
#ifdef NGB_SMOOTH_MOTION
    s+="        if(motion.x>0)p=universal_scene_positions[vertex_id];\n";
#endif
    s+=R"(
        p.w=magclamp(p.w);
        if(ext.x>0)p.x=ext.y+(p.x-ext.y)*ext.x;
        if(ext.z>0)p.y=ext.w+(p.y-ext.w)*ext.z;
        if(qsnap.x>0 && (Flag(16) || Flag(1))) {
            float2 snap=ceil(p.xy-.0001)-.5,dq=(snap-p.xy)/float2(ext.x>0?ext.x:1,ext.z>0?ext.z:1);
            for(uint t=0;t<4;t++)outputs[9+t].xy+=float2(dot(uvg[t].xy,dq),dot(uvg[t].zw,dq));
            p.xy=snap;
        }
        p.xy+=.5;precise float4 q;
        q.x=p.x*2/surf.x-1;q.y=1-p.y*2/surf.y;q.z=p.z*surf.z;q.xyz*=p.w;q.w=p.w;
        o.pos=q;o.fogamt=fogfactor(outputs[5].x);
    } else {
)";
#ifdef NGB_SMOOTH_MOTION
    s+="        if(motion.x>0)cp=universal_scene_positions[vertex_id];\n";
#endif
    s+=R"(
        o.pos=cp;o.fogamt=fogfactor(abs(cp.z));
    }
    o.d0=saturate(outputs[3]);o.d1=saturate(outputs[4]);o.b0=saturate(outputs[7]);o.b1=saturate(outputs[8]);
    o.tc0=outputs[9];o.tc1=outputs[10];o.tc2=outputs[11];o.tc3=outputs[12];
    float point_size=1;
    if(Flag(4)) {
        point_size=Flag(2)?outputs[6].x:c[190].x;
        if(!Flag(2)) {
            if(!Flag(1) && c[190].w!=0)point_size*=c[191].w*rsqrt(max(c[191].x+c[191].y*length(vp)+c[191].z*dot(vp,vp),1e-20));
            point_size=clamp(point_size,c[190].y,c[190].z);
        }
        point_size=max(point_size*surf.w,1);
    }
    o.psize=point_size;
    float2 centre=float2((o.pos.x/o.pos.w+1)*surf.x*.5,(1-o.pos.y/o.pos.w)*surf.y*.5);
    o.ptc=float4(centre*surf.w,point_size,0);
    return o;
}
)";
    return s;
}

std::string UniversalPixelHlsl(bool multisampled,bool depth_only,bool blend_in_shader) {
    std::string s=UniversalCommon(false)+PsHelpers();
    auto define=[&](const char* name,size_t at) {s+="#define "+std::string(name)+" "+std::to_string(at/4)+"u\n";};
#define FIELD(name,field) define(name,offsetof(PixelKey,field))
    FIELD("PK_PROGRAM",programmable);FIELD("PK_COUNT",count);FIELD("PK_RGB_IN",rgb_in);FIELD("PK_RGB_OUT",rgb_out);
    FIELD("PK_A_IN",a_in);FIELD("PK_A_OUT",a_out);FIELD("PK_FINAL_ABCD",final_abcd);FIELD("PK_FINAL_EFG",final_efg);
    FIELD("PK_MODES",tex_modes);FIELD("PK_DOT",dot_mapping);FIELD("PK_INPUT",input_texture);FIELD("PK_COMPARE",compare_mode);
    FIELD("PK_FF",ff);FIELD("PK_TEX",tex);FIELD("PK_KILL",alphakill);FIELD("PK_TWO",two_sided);FIELD("PK_PROJ",proj);
#undef FIELD
    for(int i=0;i<4;i++) {
        s+=F("[[vk::binding(%d,0)]] Texture2D utex2d%d;\n",2+i,i);
        s+=F("[[vk::binding(%d,0)]] Texture3D utex3d%d;\n",6+i,i);
        s+=F("[[vk::binding(%d,0)]] TextureCube utexcube%d;\n",10+i,i);
        s+=F("[[vk::binding(%d,0)]] Texture2D<float> utexdepth%d;\n",14+i,i);
        s+=F("[[vk::binding(%d,0)]] SamplerState unormal%d;\n",18+i,i);
        s+=F("[[vk::binding(%d,0)]] SamplerComparisonState ushadow%d;\n",22+i,i);
    }
    // Generate bump filtering and PCF using the same helpers as specialization.
    PixelKey bump{};for(auto& t:bump.tex){t.kind=kTex2D;t.flags=kTexSigned;}
    std::string helpers=PixelHlsl(bump);size_t start=helpers.find("float2 bumptexel0"),end=helpers.find("float4 main(",start);
    if(start==std::string::npos || end==std::string::npos)throw std::runtime_error("Bump helper generator changed");
    helpers=helpers.substr(start,end-start);
    for(int i=0;i<4;i++)helpers+=Hooks().Shadow(i)+SpreadTap(i);
    helpers=std::regex_replace(helpers,std::regex(R"(\btex([0-3])\b)"),"utex2d$1");
    helpers=std::regex_replace(helpers,std::regex(R"(\bsmp([0-3])\b)"),"unormal$1");
    // Only the shadow functions need the comparison texture/sampler names.
    for(int i=0;i<4;i++) {
        const std::string head=F("float4 shadow%d(",i);size_t a=helpers.find(head),b=helpers.find("\n}\n",a);
        if(a==std::string::npos || b==std::string::npos)throw std::runtime_error("Shadow helper generator changed");
        std::string function=helpers.substr(a,b+3-a);
        function=std::regex_replace(function,std::regex(F("\\butex2d%d\\b",i)),F("utexdepth%d",i));
        function=std::regex_replace(function,std::regex(F("\\bunormal%d\\b",i)),F("ushadow%d",i));
        helpers.replace(a,b+3-a,function);
    }
    s+=helpers;
    s+=R"(
uint Kind(uint stage) {return P(PK_TEX+stage)&255;}
uint Rect(uint stage) {return (P(PK_TEX+stage)>>8)&255;}
uint Shadow(uint stage) {return (P(PK_TEX+stage)>>16)&255;}
uint TexFlags(uint stage) {return P(PK_TEX+stage)>>24;}
float2 NormalizeUV(uint stage,float2 uv) {return (Rect(stage)!=0?uv/texsize[stage].xy:uv)+texsize[stage].zw;}
)";
    for(int i=0;i<4;i++) {
        s+=F("float4 Sample%u(float3 p,bool planar) {\n",i);
        s+=F("uint kind=Kind(%d);if(kind==1){float2 uv=NormalizeUV(%d,p.xy);return (TexFlags(%d)&4)!=0?spreadtap%d(uv,spread[%d],1):utex2d%d.Sample(unormal%d,uv);}\n",i,i,i,i,i,i,i);
        s+=F("if(kind==2)return utex3d%d.Sample(unormal%d,planar?float3(p.xy,0):p);\n",i,i);
        s+=F("if(kind==3)return utexcube%d.Sample(unormal%d,planar?float3(p.xy,1):p);return float4(0,0,0,1);}\n",i,i);
    }
    s+="float4 SampleStage(uint stage,float3 p,bool planar){switch(stage){\n";
    for(int i=0;i<4;i++)s+=F("case %d:return Sample%d(p,planar);\n",i,i);
    s+="default:return 0;}}\nfloat2 BumpStage(uint stage,float2 uv){switch(stage){\n";
    for(int i=0;i<4;i++)s+=F("case %d:return bumpduv%d(uv);\n",i,i);
    s+="default:return 0;}}\nfloat4 ShadowStage(uint stage,float2 uv,float ref,float2 pixel){switch(stage){\n";
    for(int i=0;i<4;i++)s+=F("case %d:return shadow%d(NormalizeUV(%d,uv),ref,Shadow(%d),pixel,texsize[%d].xy);\n",i,i,i,i,i);
    s+=R"(default:return 0;}}
float3 DotInputRuntime(uint mapping,float3 v) {
    if(mapping==1)return sbyte(v,128,128,127);
    if(mapping==2)return sbyte(v,-.5,255.5,127.5);
    if(mapping==3)return sbyte(v,0,256,127);
    return v;
}
float4 RegisterValue(uint reg,uint stage,float4 registers[16],float3 ef,uint final_flags) {
    uint flags=P(PK_COUNT)>>8;
    if(reg==0)return 0;
    if(reg==1)return pc[stage==8?16:(flags&16)!=0?stage*2:0];
    if(reg==2)return pc[stage==8?17:(flags&256)!=0?stage*2+1:1];
    if(reg==14) {
        float3 v1=(final_flags&64)!=0?1-registers[5].rgb:registers[5].rgb;
        float3 r0=(final_flags&32)!=0?1-registers[12].rgb:registers[12].rgb;
        return float4((final_flags&128)!=0?saturate(v1+r0):v1+r0,0);
    }
    if(reg==15)return float4(ef,0);
    return registers[reg];
}
float4 CombinerInput(uint operand,uint stage,bool alpha,float4 registers[16],float3 ef,uint final_flags) {
    float4 v=RegisterValue(operand&15,stage,registers,ef,final_flags);
    if(alpha)v=(operand&16)!=0?v.aaaa:v.bbbb;
    else if((operand&16)!=0)v=v.aaaa;
    switch(operand&224) {
    case 0:return max(v,0);case 32:return 1-saturate(v);case 64:return 2*max(v,0)-1;
    case 96:return -2*max(v,0)+1;case 128:return max(v,0)-.5;case 160:return -max(v,0)+.5;
    case 192:return v;default:return -v;
    }
}
float4 MapOutput(float4 value,uint mapping) {switch(mapping) {
)";
    for(unsigned map:{0u,8u,16u,24u,32u,40u,48u,56u})s+=F("case %u:return %s;\n",map,Ps::Map("value",map).c_str());
    s+=R"(default:return value;}}
void CombinerHalf(uint inputs,uint outputs,uint stage,bool alpha,float4 registers[16],out float4 ab,out float4 cd,out float4 sum) {
    float4 a=CombinerInput(inputs>>24,stage,alpha,registers,0,0),b=CombinerInput((inputs>>16)&255,stage,alpha,registers,0,0);
    float4 c0=CombinerInput((inputs>>8)&255,stage,alpha,registers,0,0),d=CombinerInput(inputs&255,stage,alpha,registers,0,0);
    uint flags=outputs>>12;
    float4 raw_ab=(flags&2)!=0?(alpha?a.x*b.x:dot(a.rgb,b.rgb)).xxxx:a*b;
    float4 raw_cd=(flags&1)!=0?(alpha?c0.x*d.x:dot(c0.rgb,d.rgb)).xxxx:c0*d;
    float4 raw_sum=(flags&4)!=0?(((P(PK_COUNT)>>8)&1)!=0?registers[12].a>=.5:(uint(registers[12].a*255)&1)==1)?raw_cd:raw_ab:raw_ab+raw_cd;
    ab=clamp(MapOutput(raw_ab,flags&56),-1,1);cd=clamp(MapOutput(raw_cd,flags&56),-1,1);sum=clamp(MapOutput(raw_sum,flags&56),-1,1);
}
bool Writable(uint index){return index==4 || index==5 || (index>=8 && index<=13);}
void StoreHalf(inout float4 registers[16],uint outputs,bool alpha,float4 ab,float4 cd,float4 sum) {
    uint a=(outputs>>4)&15,d=outputs&15,combined=(outputs>>8)&15,flags=outputs>>12;
    if(Writable(a)){if(alpha)registers[a].a=ab.a;else {registers[a].rgb=ab.rgb;if(flags&128)registers[a].a=ab.b;}}
    if(Writable(d)){if(alpha)registers[d].a=cd.a;else {registers[d].rgb=cd.rgb;if(flags&64)registers[d].a=cd.b;}}
    if(Writable(combined)){if(alpha)registers[combined].a=sum.a;else registers[combined].rgb=sum.rgb;}
}
float4 StageArg(uint argument,float4 v0,float4 v1,float4 cur,float4 tmp,float4 t) {
    float4 value=cur;switch(argument&15){case 0:value=v0;break;case 2:value=t;break;case 3:value=tfactor;break;case 4:value=v1;break;case 5:value=tmp;break;}
    if(argument&32)value=value.aaaa;if(argument&16)value=1-value;return value;
}
float3 ColorOp(uint operation,float4 a1,float4 a2,float4 a0,float4 v0,float4 cur,float4 t) {switch(operation) {
)";
    for(unsigned op=2;op<=24;op++) {
        auto expression=FfOp(op,"a1.rgb","a2.rgb","a0.rgb",0,false);
        expression=std::regex_replace(expression,std::regex(R"(\bt0\b)"),"t");
        s+=F("case %u:return %s;\n",op,expression.c_str());
    }
    s+="default:return cur.rgb;}}\nfloat AlphaOp(uint operation,float4 a1,float4 a2,float4 a0,float4 v0,float4 cur,float4 t) {switch(operation) {\n";
    for(unsigned op=2;op<=24;op++) {
        auto expression=FfOp(op,"a1.a","a2.a","a0.a",0,true);
        expression=std::regex_replace(expression,std::regex(R"(\bt0\b)"),"t");s+=F("case %u:return %s;\n",op,expression.c_str());
    }
    s+=R"(default:return cur.a;}}
bool UsesTexture(uint base) {
    uint op=P(base),a1=P(base+1)&15,a2=P(base+2)&15,a0=P(base+3)&15;
    if(op<=1)return false;if(op==2)return a1==2;if(op==3)return a2==2;
    if(op==14 || op==16)return true;return a1==2 || a2==2 || ((op==23 || op==24) && a0==2);
}
float4 Shade(VOut i,bool front) {
    float4 tc[4]={i.tc0,i.tc1,i.tc2,i.tc3};
    if(Flag(8))tc[3]=float4((i.pos.xy-i.ptc.xy)/i.ptc.z+.5,0,1);
    float4 v0=P(PK_TWO)!=0 && !front?i.b0:i.d0,v1=P(PK_TWO)!=0 && !front?i.b1:i.d1;
    if(misc.w==0)v1=float4(0,0,0,1);
    float4 fogin=float4(fogcolor.rgb,saturate(i.fogamt)),outc;
    if(P(PK_PROGRAM)!=0) {
        float4 t[4];float2 bump_uv[4];float dots[4];
        for(uint j=0;j<4;j++){t[j]=float4(0,0,0,1);bump_uv[j]=0;dots[j]=0;}
        for(uint j=0;j<4;j++) {
            uint mode=(P(PK_MODES)>>(j*5))&31,source=j==1?0:j==2?(P(PK_INPUT)>>16)&15:(P(PK_INPUT)>>20)&15;
            source=min(source,3u);uint mapping=j==0?0:(P(PK_DOT)>>((j-1)*4))&15;
            float3 operand=DotInputRuntime(mapping,t[source].rgb);float3 coordinates=0;bool planar=true,fetch=false;
            float2 uv=0;
            if(mode==1 || mode==2) {
                if(Shadow(j)!=0)t[j]=ShadowStage(j,tc[j].xy/tc[j].w,mode==1?0:tc[j].z/tc[j].w,i.pos.xy);
                else {coordinates=mode==1?float3(tc[j].xy/tc[j].w,0):tc[j].xyz/tc[j].w;planar=mode==1;fetch=true;}
            } else if(mode==3){coordinates=tc[j].xyz;planar=false;fetch=true;}
            else if(mode==4)t[j]=tc[j];
            else if(mode==5) {t[j]=0;for(uint lane=0;lane<4;lane++)if(((P(PK_COMPARE)>>(j*4+lane))&1)!=0?tc[j][lane]>=0:tc[j][lane]<0)discard;}
            else if(mode==6 || mode==7) {
                float2 duv=(TexFlags(source)&8)!=0?bump_uv[source]:sbyte(t[source].bgr,0,256,127).xy;
                coordinates=float3(tc[j].xy+float2(dot(bumpmat[j].xz,duv),dot(bumpmat[j].yw,duv)),0);fetch=true;
            } else if(mode==9 || mode==10 || mode==11 || mode==12 || mode==13 || mode==14 || mode==17) {
                dots[j]=dot(tc[j].xyz,operand);
                if(mode==9){coordinates=float3(dots[j>0?j-1:0],dots[j],0);fetch=true;}
                else if(mode==11) {
                    uint next_source=min((P(PK_INPUT)>>20)&15,3u);
                    float early=dot(tc[min(j+1,3u)].xyz,DotInputRuntime((P(PK_DOT)>>8)&15,t[next_source].rgb));
                    coordinates=float3(dots[j>0?j-1:0],dots[j],early);planar=false;fetch=true;
                } else if(mode==12) {
                    uint a=j>=2?j-2:0,b=j>=1?j-1:0;float3 normal=float3(dots[a],dots[b],dots[j]);
                    float3 eye=float3(tc[a].w,tc[b].w,tc[j].w);coordinates=2*normal*dot(normal,eye)/dot(normal,normal)-eye;planar=false;fetch=true;
                } else if(mode==13 || mode==14) {coordinates=float3(dots[j>=2?j-2:0],dots[j>=1?j-1:0],dots[j]);planar=false;fetch=true;}
                else t[j]=0;
            } else if(mode==15 || mode==16) {coordinates=float3(mode==15?t[source].ar:t[source].gb,0);fetch=true;}
            else t[j]=float4(0,0,0,mode==0?1:0);
            if(fetch) {
                t[j]=SampleStage(j,coordinates,planar);
                if(Kind(j)==1 && (TexFlags(j)&8)!=0)bump_uv[j]=BumpStage(j,NormalizeUV(j,coordinates.xy));
                if(mode==7)t[j]*=bumplum[j].x*t[source].r+bumplum[j].y;
            }
            bool sampled=mode!=0 && mode!=4 && mode!=5 && mode!=10 && mode!=17;
            if(sampled && ((P(PK_KILL)>>j)&1)!=0 && t[j].a==0)discard;
        }
        float4 registers[16];for(uint n=0;n<16;n++)registers[n]=0;
        registers[3]=fogin;registers[4]=v0;registers[5]=v1;for(uint n=0;n<4;n++)registers[8+n]=t[n];
        registers[12]=float4(0,0,0,(P(PK_MODES)&31)!=0?t[0].a:1);
        for(uint stage=0;stage<min(P(PK_COUNT)&255,8u);stage++) {
            float4 ab,cd,sum,aab,acd,asum;
            CombinerHalf(P(PK_RGB_IN+stage),P(PK_RGB_OUT+stage),stage,false,registers,ab,cd,sum);
            CombinerHalf(P(PK_A_IN+stage),P(PK_A_OUT+stage),stage,true,registers,aab,acd,asum);
            StoreHalf(registers,P(PK_RGB_OUT+stage),false,ab,cd,sum);
            StoreHalf(registers,P(PK_A_OUT+stage),true,aab,acd,asum);
        }
        if(P(PK_FINAL_ABCD)!=0 || P(PK_FINAL_EFG)!=0) {
            uint final_abcd=P(PK_FINAL_ABCD),final_efg=P(PK_FINAL_EFG),flags=final_efg&255;
            float3 e=CombinerInput(final_efg>>24,8,false,registers,0,flags).rgb;
            float3 f=CombinerInput((final_efg>>16)&255,8,false,registers,0,flags).rgb;
            float3 a=CombinerInput(final_abcd>>24,8,false,registers,e*f,flags).rgb;
            float3 b=CombinerInput((final_abcd>>16)&255,8,false,registers,e*f,flags).rgb;
            float3 c0=CombinerInput((final_abcd>>8)&255,8,false,registers,e*f,flags).rgb;
            float3 d=CombinerInput(final_abcd&255,8,false,registers,e*f,flags).rgb;
            outc=float4(d+lerp(c0,b,a),CombinerInput((final_efg>>8)&255,8,true,registers,e*f,flags).a);
        } else outc=float4(lerp(fogin.rgb,registers[12].rgb,fogin.a),registers[12].a);
    } else {
        float4 cur=v0,tmp=0;
        for(uint stage=Flag(8)?3:0;stage<4;stage++) {
            uint base=PK_FF+stage*9,op=P(base);if(op<=1)break;
            if(Kind(stage)==0 && (UsesTexture(base) || UsesTexture(base+4)))break;
            float2 xy=tc[stage].xy;
            if(Flag(16) && qsnap.x>0)xy=clamp(xy,uvc[stage].xy,uvc[stage].zw);
            float2 uv=((P(PK_PROJ)>>stage)&1)!=0?tc[stage].xy/tc[stage].w:xy;
            float4 t=Shadow(stage)!=0?ShadowStage(stage,tc[stage].xy/tc[stage].w,tc[stage].z/tc[stage].w,i.pos.xy):
                Kind(stage)==0?float4(1,1,1,1):SampleStage(stage,Kind(stage)==1?float3(uv,0):tc[stage].xyz,Kind(stage)==1);
            if(((P(PK_KILL)>>stage)&1)!=0 && t.a==0)discard;
            float4 a1=StageArg(P(base+1),v0,v1,cur,tmp,t),a2=StageArg(P(base+2),v0,v1,cur,tmp,t),a0=StageArg(P(base+3),v0,v1,cur,tmp,t);
            float3 rgb=saturate(ColorOp(op,a1,a2,a0,v0,cur,t));
            a1=StageArg(P(base+5),v0,v1,cur,tmp,t);a2=StageArg(P(base+6),v0,v1,cur,tmp,t);a0=StageArg(P(base+7),v0,v1,cur,tmp,t);
            float alpha=P(base+4)<=1?cur.a:saturate(AlphaOp(P(base+4),a1,a2,a0,v0,cur,t));
            if(P(base+8)==5)tmp=float4(rgb,alpha);else cur=float4(rgb,alpha);
        }
        cur.rgb=saturate(cur.rgb+v1.rgb);outc=float4(lerp(fogin.rgb,cur.rgb,fogin.a),cur.a);
    }
    alphatest(outc.a);
    outc.rgb=saturate(outc.rgb);if(Flag(128))outc.rgb*=bumpmat[3].z;return outc;
}
float4 BlendFactor(uint factor,float4 source,float4 destination,float4 constant_color) {
    switch(factor) {
    case 0:return 0;case 1:return 1;case 2:return source;case 3:return 1-source;
    case 4:return destination;case 5:return 1-destination;case 6:return source.aaaa;case 7:return 1-source.aaaa;
    case 8:return destination.aaaa;case 9:return 1-destination.aaaa;case 10:return constant_color;case 11:return 1-constant_color;
    case 12:return constant_color.aaaa;case 13:return 1-constant_color.aaaa;case 14:return float4(min(source.a,1-destination.a).xxx,1);
    default:return 1;
    }
}
float4 BlendColor(float4 source,float4 destination) {
    source=saturate(source);float4 result=source;
    if(Flag(64)) {
        float4 constant_color=asfloat(ud[45]);
        float4 a=source*BlendFactor(U(176),source,destination,constant_color),b=destination*BlendFactor(U(177),source,destination,constant_color);
        uint op=U(178);result=op==1?a-b:op==2?b-a:op==3?min(source,destination):op==4?max(source,destination):a+b;
    }
    uint mask=U(179);for(uint lane=0;lane<4;lane++)if((mask&(1u<<lane))==0)result[lane]=destination[lane];
    return saturate(result);
}
)";
    if(depth_only)s+="void main(VOut i,bool front:SV_IsFrontFace) {float4 ignored=Shade(i,front);}\n";
    else if(blend_in_shader) {
        s+=F("[[vk::binding(28,0)]] [[vk::input_attachment_index(0)]] %s<float4> destination;\n",multisampled?"SubpassInputMS":"SubpassInput");
        s+="float4 main(VOut i,bool front:SV_IsFrontFace"+std::string(multisampled?",uint sample_index:SV_SampleIndex":"")+") : SV_Target {\n";
        s+="float4 source=Shade(i,front);return BlendColor(source,destination.SubpassLoad("+std::string(multisampled?"sample_index":"")+"));}\n";
    } else s+="float4 main(VOut i,bool front:SV_IsFrontFace) : SV_Target {return Shade(i,front);}\n";
    return s;
}
