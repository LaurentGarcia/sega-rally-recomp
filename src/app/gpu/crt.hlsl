// Optional CRT presentation of an already composed frame. No frame history or
// game state; one HLSL source generates SPIR-V, DXIL and MSL.
struct CrtUniforms {
    float4 viewport; // physical output x, y, width, height
    float4 sizes;    // rendered texture width/height, native width/height
    float4 strength; // scanlines, phosphor mask, glow, curvature
    float4 extra;    // chromatic shift in native pixels, reserved
};
[[vk::binding(0, 3)]] cbuffer CrtBuffer : register(b0, space3) { CrtUniforms crt; };
[[vk::combinedImageSampler]] [[vk::binding(0, 2)]] Texture2D<float4> source_tex : register(t0, space2);
[[vk::combinedImageSampler]] [[vk::binding(0, 2)]] SamplerState source_smp : register(s0, space2);

struct CrtVertex { float4 pos : SV_Position; float2 uv : TEXCOORD0; };
CrtVertex vs_crt(uint vertex : SV_VertexID) {
    CrtVertex o;
    o.uv = float2(float((vertex << 1u) & 2u), float(vertex & 2u));
    o.pos = float4(o.uv.x * 2.0 - 1.0, 1.0 - o.uv.y * 2.0, 0.0, 1.0);
    return o;
}

float3 sample_frame(float2 uv) { return source_tex.SampleLevel(source_smp, uv, 0.0).rgb; }

float4 ps_crt(CrtVertex i) : SV_Target {
    const float2 centered = i.uv * 2.0 - 1.0;
    const float2 uv = centered * (1.0 + crt.strength.w * dot(centered, centered)) * 0.5 + 0.5;
    if (any(uv < 0.0) || any(uv > 1.0)) return float4(0.0, 0.0, 0.0, 1.0);
    float3 color = sample_frame(uv);
    if (crt.extra.x > 0.0) {
        const float2 shift = float2(crt.extra.x / crt.sizes.z, 0.0);
        color.r = sample_frame(uv + shift).r;
        color.b = sample_frame(uv - shift).b;
    }
    if (crt.strength.z > 0.0) {
        // A small spatial halo; no persistence/history, and no offscreen pass.
        const float2 d = max(1.0 / crt.sizes.xy, 1.5 / crt.sizes.zw);
        const float3 halo = (sample_frame(uv + float2(d.x, 0.0)) + sample_frame(uv - float2(d.x, 0.0)) +
                             sample_frame(uv + float2(0.0, d.y)) + sample_frame(uv - float2(0.0, d.y))) * 0.25;
        color += halo * crt.strength.z;
    }
    // Fade beam modulation when a native row occupies fewer than two output
    // pixels. This avoids inventing moire or darkening at native/downscaled size.
    // The quarter-row phase retains visible alternating rows at exact 2x.
    const float beam_resolved = saturate(crt.viewport.w / crt.sizes.w - 1.0);
    const float gap = 0.5 - 0.5 * cos(6.28318530718 * (uv.y * crt.sizes.w - 0.25));
    color *= 1.0 - crt.strength.x * beam_resolved * gap;
    // RGB phosphor stripes are one physical output pixel per subpattern,
    // anchored to the viewport, independent of source/internal resolution.
    const uint stripe = uint(max(floor(i.pos.x - crt.viewport.x), 0.0)) % 3u;
    const float3 mask = stripe == 0u ? float3(1.0, 1.0 - crt.strength.y, 1.0 - crt.strength.y) :
                        stripe == 1u ? float3(1.0 - crt.strength.y, 1.0, 1.0 - crt.strength.y) :
                                       float3(1.0 - crt.strength.y, 1.0 - crt.strength.y, 1.0);
    return float4(saturate(color * mask), 1.0);
}
