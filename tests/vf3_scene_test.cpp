#include "../third_party/vf3_scene/r3d_scene.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
using namespace video;
static void require(bool value, const char *message) {
 if (!value) { std::fprintf(stderr, "%s\n", message); std::exit(1); }
}
static Model model(uint32_t address, float x, float z, const Meshes &meshes) {
 Model m; m.addr=address; m.matrix=Mat4::identity();
 m.matrix.m[12]=x; m.matrix.m[14]=z; m.meshes=meshes; return m;
}
int main() {
 auto meshes=std::make_shared<std::vector<Mesh>>(1);
 (*meshes)[0].verts.resize(3);
 (*meshes)[0].verts[1].pos[0]=1;
 (*meshes)[0].verts[2].pos[1]=1;
 Scene a,b,out; a.id=101; b.id=102;
 a.viewports.resize(1); b.viewports.resize(1);
 for (unsigned i=0;i<8;++i)
  a.viewports[0].models.push_back(model(100+i,float(i*4),200+float(i*10),meshes));
 a.viewports[0].models.push_back(model(900,-20,30,meshes));
 a.viewports[0].models.push_back(model(900,20,30,meshes));
 b.viewports[0]=a.viewports[0];
 for (auto &m:b.viewports[0].models) m.matrix.m[12]-=1;
 std::swap(b.viewports[0].models[8],b.viewports[0].models[9]);
 Mat4 camera;
 require(camera_move(a.viewports[0],b.viewports[0],camera),"camera consensus failed");
 require(std::fabs(camera.m[12]+1)<1e-5f,"camera translation differs");
 require(interpolate(a,b,.5f,out),"scene interpolation rejected");
 require(std::fabs(out.viewports[0].models[8].matrix.m[12]-19.5f)<1e-5f,"reordered right instance paired incorrectly");
 require(std::fabs(out.viewports[0].models[9].matrix.m[12]+20.5f)<1e-5f,"reordered left instance paired incorrectly");
 require(interpolate(a,b,.25f,out),"cached scene interpolation rejected");
 require(std::fabs(out.viewports[0].models[8].matrix.m[12]-19.75f)<1e-5f,"cached pairing changed");
 std::puts("VF3 unchanged core: camera inference, repeated-resource reordering, cached interpolation passed");
}
