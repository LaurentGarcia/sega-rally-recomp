// Static geometry/readback fixture. No arcade runtime or driving systems.
// Input and PPM output remain owner-provided/private host files.
#include "native_display.h"
#include "native_mesh.h"
#include "native_scene.h"
#include <kernel.h>
#include <dma_registers.h>
#include <sifrpc.h>
#include <array>
#include <cstdio>
#include <exception>

int main(int argc, char **argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    const auto dma_before=*DMA_REG_CTRL;
    *DMA_REG_CTRL=dma_before|1u;
    asm volatile("sync.l" ::: "memory");
    std::printf("SCENE FIXTURE DMAE before=%08lx after=%08lx\n",
        static_cast<unsigned long>(dma_before),static_cast<unsigned long>(*DMA_REG_CTRL));
    if (*DMA_REG_CTRL!=(dma_before|1u)) { std::puts("SCENE FIXTURE DMAE SETUP FAIL");return 1; }
    SifInitRpc(0); // Preserve the loader's host filesystem service.
    const char *input=argc>1?argv[1]:"host:native-scene.bin";
    const char *output=argc>2?argv[2]:"host:native-study-frame.ppm";
    ps2::native::Display display;
    try {
        if (!display.initialize()) { std::printf("SCENE CAPTURE INIT FAIL %s\n",display.error()); return 1; }
        ps2::native::Mesh mesh;
        ps2::native::MeshError mesh_error;
        if (!ps2::native::load_mesh_file(input,mesh,mesh_error)) {
            std::printf("SCENE CAPTURE LOAD FAIL %s\n",ps2::native::mesh_error_message(mesh_error));return 1;
        }
        ps2::native::StudyScene scene;
        ps2::native::StudyError scene_error;
        if (!scene.prepare(mesh,{},scene_error)) {
            std::printf("SCENE CAPTURE PREPARE FAIL %s\n",ps2::native::study_error_message(scene_error));return 1;
        }
        display.set_caption("STATIC GEOMETRY STUDY\nDIAGNOSTIC CAMERA / FLAT SHADING");
        // The dump is the completed CT32 framebuffer before TV filtering.
        if (!display.present(scene.frame())) { std::printf("SCENE CAPTURE DRAW FAIL %s\n",display.error());return 1; }
        if (FILE *existing=std::fopen(output,"rb")) {
            std::fclose(existing);std::puts("SCENE CAPTURE REFUSES EXISTING OUTPUT");return 1;
        }
        FILE *file=std::fopen(output,"wb");
        if (!file) { std::puts("SCENE CAPTURE OUTPUT OPEN FAIL");return 1; }
        std::array<std::uint32_t,640*16> pixels{};
        std::array<unsigned char,640*16*3> rgb{};
        bool complete=std::fprintf(file,"P6\n640 %u\n255\n",display.output_height())>0;
        for (unsigned y=0; complete && y<display.output_height(); y+=16) {
            const auto status=display.readback_rect(0,y,640,16,pixels);
            if (status!=ps2::native::ReadbackStatus::ok) {
                std::printf("SCENE CAPTURE READBACK FAIL y=%u status=%u\n",y,unsigned(status));
                complete=false;break;
            }
            for (unsigned i=0;i<pixels.size();++i) {
                rgb[3*i]=pixels[i]&255;rgb[3*i+1]=(pixels[i]>>8)&255;rgb[3*i+2]=(pixels[i]>>16)&255;
            }
            complete=std::fwrite(rgb.data(),1,rgb.size(),file)==rgb.size();
        }
        if (std::fclose(file)!=0) complete=false;
        if (!complete) { std::puts("SCENE CAPTURE INCOMPLETE; DO NOT USE OUTPUT");return 1; }
        std::printf("SCENE CAPTURE PASS path=%s crc=%08lx vertices=%lu objects=%lu triangles=%lu width=640 height=%u; static diagnostic GS framebuffer\n",
            output,static_cast<unsigned long>(mesh.payload_crc32),static_cast<unsigned long>(mesh.vertices.size()),
            static_cast<unsigned long>(mesh.objects.size()),static_cast<unsigned long>(scene.stats().output_triangles),display.output_height());
        while (true) SleepThread();
    } catch (const std::exception &e) { std::printf("SCENE CAPTURE FAIL %s\n",e.what());return 1; }
}
