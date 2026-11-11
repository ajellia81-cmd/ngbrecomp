#pragma once
#include <array>
#include <cstdint>
#include <vector>

namespace gpu {
struct MotionClipPoint { int instruction=-1,reg=-1; };
namespace motion_clip_detail {
struct Token {
    const uint32_t* words;
    unsigned f(unsigned word,unsigned bit,unsigned count)const{return (words[word]>>bit)&((1u<<count)-1);}
    unsigned mac()const{return f(1,21,4);}
    unsigned ilu()const{return f(1,25,3);}
};
struct Operand {unsigned mux=0,reg=0,constant=0,swizzle=0;bool negative=false,relative=false;};
inline Operand operand(Token k,unsigned which){
    Operand o;o.constant=k.f(1,13,8);o.relative=k.f(3,1,1)!=0;
    unsigned sw[4];
    if(which==0){o.mux=k.f(2,26,2);o.reg=k.f(2,28,4);o.negative=k.f(1,8,1);for(unsigned i=0;i<4;i++)sw[i]=k.f(1,6-i*2,2);}
    else if(which==1){o.mux=k.f(2,11,2);o.reg=k.f(2,13,4);o.negative=k.f(2,25,1);for(unsigned i=0;i<4;i++)sw[i]=k.f(2,23-i*2,2);}
    else{o.mux=k.f(3,28,2);o.reg=(k.f(2,0,2)<<2)|k.f(3,30,2);o.negative=k.f(2,10,1);for(unsigned i=0;i<4;i++)sw[i]=k.f(2,8-i*2,2);if(k.ilu()>=2&&k.ilu()<=6)for(unsigned i=1;i<4;i++)sw[i]=sw[0];}
    for(unsigned i=0;i<4;i++)o.swizzle=(o.swizzle<<2)|sw[i];
    return o;
}
inline bool temporary(Operand o,unsigned reg,unsigned swizzle=0x1b){return o.mux==1&&o.reg==reg&&!o.negative&&o.swizzle==swizzle;}
inline bool constant(Operand o,unsigned reg){return o.mux!=1&&o.mux!=2&&!o.relative&&!o.negative&&o.constant==reg&&o.swizzle==0x1b;}
struct Writes {unsigned mac=0,ilu=0;unsigned all()const{return mac|ilu;}};
inline Writes writes(Token k,unsigned reg){
    Writes w;unsigned dest=k.f(3,20,4);
    if(k.mac()&&k.mac()!=13&&dest==reg&&!(k.ilu()&&dest==1))w.mac|=k.f(3,24,4);
    if(k.ilu()&&(k.mac()?1:dest)==reg)w.ilu|=k.f(3,16,4);
    if(reg==12&&k.f(3,11,1)&&k.f(3,3,8)%16==0){
        if(k.f(3,2,1)){if(k.ilu())w.ilu|=k.f(3,12,4);}
        else if(k.mac()&&k.mac()!=13)w.mac|=k.f(3,12,4);
    }
    return w;
}
inline bool reads(Token k,unsigned reg){
    unsigned used=k.ilu()?4:0;
    switch(k.mac()){
    case 0:break;case 1:case 13:used|=1;break;case 3:used|=5;break;case 4:used|=7;break;default:used|=3;break;
    }
    for(unsigned i=0;i<3;i++)if(used&(1u<<i)){auto o=operand(k,i);if(o.mux==1&&o.reg==reg)return true;}
    return false;
}
}
// Recognize a complete clip-position temporary followed by the console's
// c58/c59 viewport normalization. Replacing that temporary before its parallel
// copy to oPos also updates screen-derived UV/fog calculations. Ambiguous
// dataflow, early consumers, changed reciprocal lanes and other position
// writers deliberately retain the ordinary position-only wrapper.
inline MotionClipPoint FindMotionClipPoint(const std::vector<uint32_t>& program){
    using namespace motion_clip_detail;
    size_t count=program.size()/4,limit=count>136?136:count;bool ended=false;
    for(size_t i=0;i<limit;i++)if(Token{program.data()+i*4}.f(3,0,1)){count=i+1;ended=true;break;}
    if(count>136&&!ended)return {};
    auto token=[&](size_t i){return Token{program.data()+i*4};};
    for(size_t copy=0;copy<count;copy++){
        auto k=token(copy);auto pos=writes(k,12);Operand source;
        if(pos.mac==15&&!pos.ilu&&k.mac()==1)source=operand(k,0);
        else if(pos.ilu==15&&!pos.mac&&k.ilu()==1)source=operand(k,2);
        else continue;
        if(source.mux!=1||source.reg>=12||source.negative||source.swizzle!=0x1b)continue;
        unsigned covered=0;size_t begin=copy;
        while(begin&&covered!=15){--begin;covered|=writes(token(begin),source.reg).all();}
        if(covered!=15)continue;
        bool safe=true;
        for(size_t i=begin;i<copy;i++)if(reads(token(i),source.reg)){safe=false;break;}
        if(!safe)continue;
        int stage=0;unsigned reciprocal=0,modified=0;
        for(size_t i=copy+1;i<count;i++){
            auto next=token(i);auto output=writes(next,12);
            if(!output.all()){
                if(stage==1)modified|=writes(next,1).all();
                continue;
            }
            auto a=operand(next,0),b=operand(next,1),c=operand(next,2);
            if(stage==0&&output.mac==14&&!output.ilu&&next.mac()==2&&
               ((temporary(a,12)&&constant(b,58))||(temporary(b,12)&&constant(a,58)))&&
               (next.ilu()==2||next.ilu()==3)&&temporary(c,12,0xff)&&next.f(3,16,4)){
                reciprocal=next.f(3,16,4);stage=1;continue;
            }
            auto reciprocal_source=[&](Operand o){
                if(o.mux!=1||o.reg!=1||o.negative)return false;
                unsigned lane=o.swizzle&3;
                return o.swizzle==lane*0x55&&((reciprocal&~modified)&(8u>>lane));
            };
            if(stage==1&&output.mac==14&&!output.ilu&&next.mac()==4&&constant(c,59)&&
               ((temporary(a,12)&&reciprocal_source(b))||(temporary(b,12)&&reciprocal_source(a)))){stage=2;continue;}
            safe=false;break;
        }
        if(safe&&stage==2)return {int(copy),int(source.reg)};
    }
    return {};
}
}
