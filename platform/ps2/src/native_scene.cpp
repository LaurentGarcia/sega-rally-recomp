#include "native_scene.h"
#include "native_geometry.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <new>

namespace ps2::native {
namespace {
struct Point { double x, y, z; };
bool accepted(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value) & 0x7fffffffu;
    return bits == 0 || (bits >= 0x00800000u && bits <= 0x47800000u);
}
Point transform(const MeshVertex &v, const MeshObject &object) {
    const auto &m = object.object_to_reference_view;
    return {double(m[0])*v.x + double(m[4])*v.y + double(m[8])*v.z + m[12],
            double(m[1])*v.x + double(m[5])*v.y + double(m[9])*v.z + m[13],
            double(m[2])*v.x + double(m[6])*v.y + double(m[10])*v.z + m[14]};
}
bool finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }
bool valid_mesh(const Mesh &mesh) {
    if (mesh.vertices.empty() || mesh.faces.empty() || mesh.objects.empty() ||
        mesh.vertices.size() > mesh_vertex_limit || mesh.faces.size() > mesh_face_limit ||
        mesh.objects.size() > mesh_object_limit) return false;
    for (const auto &v : mesh.vertices)
        if (!accepted(v.x) || !accepted(v.y) || !accepted(v.z)) return false;
    std::size_t vertices = 0, faces = 0;
    for (const auto &o : mesh.objects) {
        if (o.first_vertex != vertices || o.first_face != faces || o.vertex_count < 3 || o.face_count == 0 ||
            o.vertex_count > mesh.vertices.size() - vertices || o.face_count > mesh.faces.size() - faces) return false;
        for (float value : o.object_to_reference_view) if (!accepted(value)) return false;
        const auto &m = o.object_to_reference_view;
        if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1) return false;
        for (std::size_t f = faces; f < faces + o.face_count; ++f) {
            const auto &face = mesh.faces[f];
            if ((face.argb >> 24) != 255 || face.indices[0] == face.indices[1] ||
                face.indices[1] == face.indices[2] || face.indices[0] == face.indices[2]) return false;
            for (auto index : face.indices)
                if (index < vertices || index >= vertices + o.vertex_count) return false;
        }
        vertices += o.vertex_count; faces += o.face_count;
    }
    return vertices == mesh.vertices.size() && faces == mesh.faces.size();
}
std::uint32_t shade(std::uint32_t color, const std::array<Point, 3> &v) {
    const Point a{v[1].x-v[0].x,v[1].y-v[0].y,v[1].z-v[0].z};
    const Point b{v[2].x-v[0].x,v[2].y-v[0].y,v[2].z-v[0].z};
    const Point n{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
    const double length = std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z);
    // Original study lighting; abs() keeps unknown winding visible. No game
    // material/lighting recovery or backface-culling claim is made here.
    const double light = length > 0 ? .28 + .72 * std::fabs((-.3*n.x+.4*n.y-.8660254037844386*n.z)/length) : .28;
    std::uint32_t result = 0xff000000u;
    for (unsigned shift : {0u,8u,16u})
        result |= std::uint32_t(std::clamp(double((color >> shift) & 255u)*light+.5,0.,255.)) << shift;
    return result;
}
}

const char *study_error_message(StudyError error) noexcept {
    switch (error) {
    case StudyError::none: return "NONE";
    case StudyError::invalid_mesh: return "INVALID NATIVE STUDY MESH";
    case StudyError::invalid_camera: return "INVALID DIAGNOSTIC CAMERA";
    case StudyError::degenerate_bounds: return "NATIVE MESH HAS ZERO EXTENT";
    case StudyError::transform_failed: return "NATIVE MESH TRANSFORM FAILED";
    case StudyError::output_limit: return "NATIVE STUDY EXCEEDS 16384 OUTPUT TRIANGLES";
    case StudyError::clipping_failed: return "NATIVE STUDY CLIPPING FAILED";
    default: return "NATIVE STUDY ALLOCATION FAILED";
    }
}

bool StudyScene::prepare(const Mesh &mesh, const StudyCamera &camera, StudyError &error) noexcept {
    error = StudyError::none;
    if (!valid_mesh(mesh)) { error=StudyError::invalid_mesh; return false; }
    if (!accepted(camera.yaw) || !accepted(camera.pitch) || !accepted(camera.distance) ||
        std::fabs(camera.yaw)>3.141593f || std::fabs(camera.pitch)>3.141593f ||
        camera.distance<1.5f || camera.distance>12.f) { error=StudyError::invalid_camera; return false; }
    Point low{std::numeric_limits<double>::max(),std::numeric_limits<double>::max(),std::numeric_limits<double>::max()};
    Point high{-low.x,-low.y,-low.z};
    for (const auto &object : mesh.objects)
        for (std::size_t i=object.first_vertex;i<std::size_t(object.first_vertex)+object.vertex_count;++i) {
            const auto p=transform(mesh.vertices[i],object);
            if (!finite(p)) { error=StudyError::transform_failed; return false; }
            low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
            high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};
        }
    const Point center{(low.x+high.x)*.5,(low.y+high.y)*.5,(low.z+high.z)*.5};
    const Point extent{(high.x-low.x)*.5,(high.y-low.y)*.5,(high.z-low.z)*.5};
    const double radius=std::sqrt(extent.x*extent.x+extent.y*extent.y+extent.z*extent.z);
    if (!(radius>0) || !std::isfinite(radius)) { error=StudyError::degenerate_bounds; return false; }
    const double cy=std::cos(double(camera.yaw)),sy=std::sin(double(camera.yaw));
    const double cp=std::cos(double(camera.pitch)),sp=std::sin(double(camera.pitch));
    const double near=.25,far=double(camera.distance)+2.,focal=1.7320508075688772;
    const auto view=[&](Point p) {
        const double x=(p.x-center.x)/radius,y=(p.y-center.y)/radius,z=(p.z-center.z)/radius;
        const double rx=cy*x+sy*z,rz=-sy*x+cy*z;
        return Point{rx,cp*y-sp*rz,sp*y+cp*rz+camera.distance};
    };
    try {
        // Validated caps are <=32768 materials and16384 triangles, many orders
        // below vector::max_size on32-bit EE. No unbounded lengths reach STL.
        std::vector<Material> materials(mesh.faces.size());
        std::vector<Triangle> triangles;
        triangles.reserve(study_triangle_limit);
        StudyStats stats{std::uint32_t(mesh.vertices.size()),std::uint32_t(mesh.faces.size()),
                         std::uint32_t(mesh.objects.size()),0,0,0};
        for (const auto &object : mesh.objects)
            for (std::size_t f=object.first_face;f<std::size_t(object.first_face)+object.face_count;++f) {
                const auto &face=mesh.faces[f];
                std::array<Point,3> positions;
                for (unsigned k=0;k<3;++k) positions[k]=view(transform(mesh.vertices[face.indices[k]],object));
                auto &material=materials[f];
                material.color=camera.diagnostic_lighting ? shade(face.argb,positions) : face.argb;
                ClipTriangle input; input.material=&material;
                for (unsigned k=0;k<3;++k) {
                    const auto &p=positions[k];
                    input.vertices[k]={float(p.x*focal*frame_height/frame_width),float(p.y*focal),
                        float(far/(far-near)*p.z-far*near/(far-near)),float(p.z),0,0};
                }
                std::array<Triangle,clip_triangle_capacity> clipped;
                const auto result=clip_project_triangle(input,clipped);
                if (result.status!=ClipStatus::success) { error=StudyError::clipping_failed; return false; }
                if (result.count>study_triangle_limit-triangles.size()) { error=StudyError::output_limit; return false; }
                if (!result.count) ++stats.clipped_faces;
                triangles.insert(triangles.end(),clipped.begin(),clipped.begin()+result.count);
            }
        stats.output_triangles=std::uint32_t(triangles.size());
        stats.persistent_bytes=materials.capacity()*sizeof(Material)+triangles.capacity()*sizeof(Triangle);
        materials_.swap(materials); triangles_.swap(triangles); stats_=stats;
        return true;
    } catch (const std::bad_alloc &) { error=StudyError::allocation_failed; return false; }
}
} // namespace ps2::native
