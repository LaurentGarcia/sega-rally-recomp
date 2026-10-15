# Native Vulkan CRT bring-up

This original ROM-free fixture creates a Vulkan 1.1 instance/device, uploads an invented 64×48 RGBA pattern, and draws the existing production CRT SPIR-V with native Vulkan descriptor sets, images, sampler, render pass and pipeline. There is no SDL include, link or runtime. It validates every output channel for identity settings and a 0.5 phosphor mask against scalar expectations, allowing one UNORM unit for rounding. The Python runner independently checks all persisted raw readback bytes too.

On Linux, install Vulkan development files and a Vulkan driver (`libvulkan-dev mesa-vulkan-drivers` on Ubuntu). The workflow selects Mesa's lavapipe software ICD and explicitly requires a CPU device. This proves software Vulkan execution only, not hardware fidelity or speed. On Windows install a Vulkan SDK and compatible Vulkan driver, then use the same CMake/Python commands; Windows execution remains unverified until separately measured.

```sh
python3 tests/test_native_vulkan_crt.py --output build/native-vulkan-001
# Linux software-only gate (with the corresponding Mesa ICD selected):
VK_DRIVER_FILES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json python3 tests/test_native_vulkan_crt.py --output build/native-vulkan-lavapipe-001 --require-software
```

The output must be new. Build and execution have separate timeouts; the native fixture additionally waits at most five seconds per submission. Two draws/readbacks run once. Failed evidence is preserved, with no retry. No OS surface, full Model 2 renderer, game app, input, sound or production migration is implemented here. Existing SDL releases remain unchanged.
