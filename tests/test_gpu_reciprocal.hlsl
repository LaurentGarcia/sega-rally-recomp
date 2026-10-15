// Manual ROM-free fixture: compile these entries with src/ as include root.
#include "app/gpu/normal_reciprocal.hlsl"
// Each operand is input bits/significand, seed bits, mode, unique tag.
StructuredBuffer<uint4> operands : register(t0, space2);
float4 vs_arithmetic(uint vertex : SV_VertexID) : SV_Position {
    const float2 corners[3] = {float2(-1,-1),float2(-1,3),float2(3,-1)};
    return float4(corners[vertex],0,1);
}
uint4 ps_arithmetic(float4 position : SV_Position) : SV_Target0 {
    const uint4 row = operands[uint(position.y)*256u+uint(position.x)];
    if (row.z == 1u)
        return uint4(reciprocal_certified_significand(row.x,row.y),
                     reciprocal_exact_significand(row.x),row.x,row.w);
    const float x = asfloat(row.x);
    return uint4(asuint(normal_reciprocal_or_legacy(x)),asuint(1.0/x),row.x,row.w);
}
// Separate compiled entry point ensures excluded-class equivalence is checked
// against an actual legacy-only shader, not only a shared local expression.
uint4 ps_baseline(float4 position : SV_Position) : SV_Target0 {
    const uint4 row = operands[uint(position.y)*256u+uint(position.x)];
    const uint value = asuint(1.0/asfloat(row.x));
    return uint4(value,value,row.x,row.w);
}
