#!/usr/bin/env python3
"""Generate only crt_shaders_gen.h, using the common pinned shader toolchain."""
import os
import build_shaders as common

common.SRC = "src/app/gpu/crt.hlsl"
common.OUT = os.path.join(common.ROOT, "src", "app", "gpu", "crt_shaders_gen.h")
common.WORK = "build/crt-shaders"
common.ENTRIES = [("vs_crt", "vertex", "vs_6_0"), ("ps_crt", "fragment", "ps_6_0")]
common.main()
# The common generator's descriptive header names the polygon input.
with open(common.OUT) as source:
    generated = source.read()
generated = generated.replace("scripts/build_shaders.py from src/app/gpu/m2.hlsl",
                              "scripts/build_crt_shaders.py from src/app/gpu/crt.hlsl", 1)
with open(common.OUT, "w") as target:
    target.write(generated)
