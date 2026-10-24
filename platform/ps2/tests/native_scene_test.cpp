#include "native_scene.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace ps2::native;
Mesh original() {
    Mesh mesh;
    mesh.vertices={{-1,-1,0},{1,-1,0},{0,1,0}};
    mesh.faces={{{0,1,2},0xffff8040u}};
    MeshObject object; object.vertex_count=3; object.face_count=1;
    object.object_to_reference_view={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    mesh.objects.push_back(object);
    return mesh;
}
int main() {
    auto mesh=original(); StudyScene scene; StudyError error; StudyCamera camera;
    camera.diagnostic_lighting=false;
    assert(scene.prepare(mesh,camera,error) && error==StudyError::none);
    auto frame=scene.frame(); const char *failure;
    assert(frame.depth_test && frame.triangles.size()==1 && valid_frame(frame,failure));
    const auto before=frame.triangles[0];
    assert(before.material->color==0xffff8040u && before.material->texture==nullptr);
    assert(std::fabs(before.vertices[0].x+before.vertices[1].x-496)<.0001);
    assert(before.vertices[0].y==before.vertices[1].y);
    assert(before.vertices[2].x==248 && before.vertices[2].y<before.vertices[0].y);
    const double expected_depth=.25*2/(2.75*4.5);
    for (const auto &v:before.vertices) {
        assert(std::fabs(v.q-1/2.75)<.000001);
        assert(std::fabs(v.depth-expected_depth)<.000001);
    }
    // A uniform reference-view translation/scale is removed by the explicit
    // diagnostic fit. This checks matrix layout independently of projection.
    mesh.objects[0].object_to_reference_view={2,0,0,0,0,2,0,0,0,0,2,0,10,-4,2,1};
    assert(scene.prepare(mesh,camera,error));
    for (unsigned i=0;i<3;++i) {
        const auto &v=scene.frame().triangles[0].vertices[i];
        assert(v.x==before.vertices[i].x && v.y==before.vertices[i].y && v.depth==before.vertices[i].depth);
    }
    const auto *retained=scene.frame().triangles.data();
    const auto check_rejected=[&](StudyError wanted) {
        assert(!scene.prepare(mesh,camera,error) && error==wanted);
        assert(scene.frame().triangles.data()==retained && scene.frame().triangles.size()==1);
    };
    camera.distance=0; check_rejected(StudyError::invalid_camera); camera.distance=2.75f;
    camera.yaw=std::numeric_limits<float>::quiet_NaN(); check_rejected(StudyError::invalid_camera); camera.yaw=0;
    mesh.faces[0].indices[2]=3; check_rejected(StudyError::invalid_mesh); mesh=original();
    mesh.objects[0].object_to_reference_view[3]=1; check_rejected(StudyError::invalid_mesh); mesh=original();
    mesh.objects[0].first_vertex=1; check_rejected(StudyError::invalid_mesh); mesh=original();
    mesh.vertices[0].x=std::numeric_limits<float>::infinity(); check_rejected(StudyError::invalid_mesh); mesh=original();
    for (auto &v:mesh.vertices) v={0,0,0}; check_rejected(StudyError::degenerate_bounds); mesh=original();
    mesh.faces.resize(study_triangle_limit+1,mesh.faces[0]);
    mesh.objects[0].face_count=mesh.faces.size(); check_rejected(StudyError::output_limit); mesh=original();
    for (unsigned step=0;step<72;++step) {
        camera.yaw=float(step)*.08f-2.84f; camera.pitch=float(int(step%15)-7)*.1f;
        assert(scene.prepare(mesh,camera,error));
        assert(valid_frame(scene.frame(),failure));
        assert(scene.stats().source_faces==1 && scene.stats().source_vertices==3);
        assert(scene.stats().output_triangles<=8);
    }
    // Two asymmetric affine objects, checked against explicit reference-view
    // points rather than reproducing production matrix multiplication.
    mesh=original();
    mesh.vertices.insert(mesh.vertices.end(),{{-1,-1,0},{1,-1,0},{0,1,0}});
    mesh.faces.push_back({{3,4,5},0xff40a0ffu});
    mesh.objects[0].object_to_reference_view={1,0,0,0,.5f,1,0,0,0,0,1,0,-2,0,0,1};
    auto second=mesh.objects[0]; second.first_vertex=3; second.first_face=1;
    second.object_to_reference_view={0,1,0,0,1,0,0,0,0,0,1,0,2,1,1,1};
    mesh.objects.push_back(second); camera={0,0,2.75f,false};
    assert(scene.prepare(mesh,camera,error) && scene.frame().triangles.size()==2);
    const double points[6][3]={{-3.5,-1,0},{-1.5,-1,0},{-1.5,1,0},{1,0,1},{1,2,1},{3,1,1}};
    const double center[3]={-.25,.5,.5},radius=std::sqrt(3.25*3.25+1.5*1.5+.5*.5);
    for (unsigned i=0;i<6;++i) {
        const double x=(points[i][0]-center[0])/radius,y=(points[i][1]-center[1])/radius;
        const double z=(points[i][2]-center[2])/radius+2.75;
        const auto &v=scene.frame().triangles[i/3].vertices[i%3];
        assert(std::fabs(v.x-(248+192*std::sqrt(3.)*x/z))<.00005);
        assert(std::fabs(v.y-(192-192*std::sqrt(3.)*y/z))<.00005);
        assert(std::fabs(v.depth-(.25*(4.75-z)/(z*4.5)))<.000001);
    }
    std::puts("NATIVE SCENE PASS: projection, matrix fit, orbit, bounds and retained frame on failure");
}
