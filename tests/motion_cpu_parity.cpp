// Differential fixture against the previously shipped CPU evaluator. No game data.
#define main ngb_existing_scene_fixture_main
#include "../ios/motion_test.cpp"
#undef main
#include <bit>
#include <chrono>
#include <cstdint>
static uint64_t checksum=1469598103934665603ull;
static void bytes(const void* value,size_t count){auto p=static_cast<const uint8_t*>(value);for(size_t i=0;i<count;i++){checksum^=p[i];checksum*=1099511628211ull;}}
static void result(bool valid,const ScenePosition& value){bytes(&valid,sizeof valid);if(valid)bytes(value.data(),sizeof value);}
int main(){
 uint32_t random=0x12345678;
 auto number=[&](){random=random*1664525u+1013904223u;return float(int(random>>8)%4097-2048)/256;};
 for(unsigned mac=1;mac<=12;mac++)for(unsigned ilu=0;ilu<=7;ilu++)for(unsigned output=0;output<2;output++)for(unsigned sample=0;sample<32;sample++){
  State s;s.vs_handle=1;s.vs=std::make_shared<VertexShader>();
  instruction(s.vs->program,mac,ilu,20,2,15,{2},{3},{3},15,15,output!=0);s.vs->program.back()|=1;
  float input[4];for(unsigned lane=0;lane<4;lane++){input[lane]=number();s.vsc[20][lane]=number();}
  if(sample==0){input[0]=-0.f;s.vsc[20][0]=0;}
  if(sample==1){input[0]=std::numeric_limits<float>::infinity();s.vsc[20][0]=0;}
  if(sample&1)s.vs->program[1]|=1u<<8;
  ScenePosition out{};DrawConstants dc{};result(SceneVertexPosition(s,geometry(input,1),0,dc,out),out);
 }
 for(int offset:{-30,0,2,180,250}){
  State s;s.vs_handle=1;s.vs=std::make_shared<VertexShader>();
  instruction(s.vs->program,13,0,20,0,0,{3});
  instruction(s.vs->program,1,0,21,0,0,{3},{},{},0,15,false,true);s.vs->program.back()|=1;
  s.vsc[20]={float(offset),0,0,0};for(unsigned c=0;c<192;c++)if(c!=20)s.vsc[c]={float(c),2,3,1};
  float input[]={0,0,0,1};ScenePosition out{};DrawConstants dc{};result(SceneVertexPosition(s,geometry(input,1),0,dc,out),out);
 }
 // Exercise per-draw folding in the actual Add path, including vertex-dependent
 // relative sources and changed constants/matrices in successive draws/frames.
 double add_ms=0,resolve_ms=0;
 for(bool relative:{false,true}){
  auto code=position_program();
  for(unsigned i=0;i<4;i++)code[i*4+2]=(code[i*4+2]&~((3u<<26)|(15u<<28)))|(1u<<26)|(2u<<28);
  std::vector<uint32_t> prefix;
  if(relative)instruction(prefix,13,0,0,0,0,{2});
  instruction(prefix,1,0,0,2,15,{2});
  if(relative)instruction(prefix,3,0,20,2,8,{1,2},{},{3},0,0,false,true);
  instruction(prefix,2,0,0,2,8,{1,2},{1,2});prefix.insert(prefix.end(),code.begin(),code.end());
  constexpr unsigned models=8,passes=2,count=4095;
  std::vector<std::vector<float>> inputs(models,std::vector<float>(count*4));
  for(unsigned m=0;m<models;m++)for(unsigned i=0;i<count;i++){
   inputs[m][i*4]=float(i%91)*.03f+m*.001f;inputs[m][i*4+1]=float(i/91)*.01f;inputs[m][i*4+2]=float(i%3)*.002f;inputs[m][i*4+3]=1;
  }
  SceneMotion history;
  for(unsigned frame=0;frame<4;frame++){
   std::vector<SceneOutput> out(models*passes);
   for(unsigned m=0;m<models;m++)for(unsigned pass=0;pass<passes;pass++){
    auto world=Mat::identity();world.m[12]=float(m*4);world.m[14]=100+float(m*10);
    auto s=state(world,-float(frame)*.1f);s.vs->program=prefix;
    for(unsigned c=20;c<24;c++)s.vsc[c]={float(c-19)*.1f+frame*.01f,0,0,0};
    auto a=geometry(inputs[m].data(),count);DrawConstants dc{};
    auto begin=std::chrono::steady_clock::now();bool ok=history.Add(s,a,dc,out[m*passes+pass]);add_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    bytes(&ok,sizeof ok);check(ok,"nonlinear fixture rejected");
   }
   auto begin=std::chrono::steady_clock::now();auto stats=history.Resolve();resolve_ms+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
   bytes(&stats.changed,sizeof stats.changed);
   for(const auto& output:out){size_t n=output.positions.size();bytes(&n,sizeof n);if(n)bytes(output.positions.data(),n*sizeof(ScenePosition));}
   history.Finish();
  }
 }
 std::cout<<"parity="<<std::hex<<checksum<<std::dec<<"\nadd_ms="<<add_ms<<" resolve_ms="<<resolve_ms<<"\n";
}
