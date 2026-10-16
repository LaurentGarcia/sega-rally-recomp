// Objective-C objects stay borrowed across this C++ host boundary.
#pragma once
struct ImDrawData;
namespace app::display_metal {
bool init(void *device);
void shutdown();
bool new_frame(void *device, void *render_pass_descriptor);
bool render(ImDrawData *, void *device, void *command_buffer, void *render_encoder);
}
