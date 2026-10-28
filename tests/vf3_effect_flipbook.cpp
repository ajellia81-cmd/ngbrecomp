// Exercise the actual internal flipbook pass, not a reimplementation.
#include "../third_party/vf3_scene/interpolate.cpp"
#include <cassert>
int main() {
 using namespace video;
 Scene b,out;b.viewports.resize(1);b.viewports[0].models.resize(1);
 Mesh oldmesh,newmesh;oldmesh.textured=newmesh.textured=true;
 oldmesh.width=newmesh.width=64;oldmesh.height=newmesh.height=64;
 oldmesh.x=0;newmesh.x=64;oldmesh.verts.resize(3);newmesh.verts.resize(3);
 oldmesh.verts[0].pos[0]=1;newmesh.verts[0].pos[0]=9;
 auto before=std::make_shared<std::vector<Mesh>>(1,oldmesh);
 auto after=std::make_shared<std::vector<Mesh>>(1,newmesh);
 b.viewports[0].models[0].meshes=after;
 plan={};plan.ok=true;plan.flips.resize(1);plan.flips[0].resize(1);
 plan.from={{0}};plan.swarm={{0}};
 plan.flips[0][0].from=before;plan.flips[0][0].age=0;plan.flips[0][0].period=3;
 out=b;apply_flips(b,0.5f,out);
 assert(out.viewports[0].models[0].meshes!=after); // ordinary water still blends
 plan.swarm[0][0]=1;
 out=b;apply_flips(b,0.5f,out);
 assert(out.viewports[0].models[0].meshes==after); // excluded sand must stay untouched
 plan.swarm[0][0]=0;plan.from[0][0]=-1;
 out=b;apply_flips(b,0.5f,out);
 assert(out.viewports[0].models[0].meshes==after); // unpaired/snapped models too
 puts("PASS: flipbook respects particle/snap exclusion; stable water animation preserved");
}
