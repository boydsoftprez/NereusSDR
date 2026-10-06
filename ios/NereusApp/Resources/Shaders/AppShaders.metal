// NereusSDR for iOS: the app's Metal shaders, shipped as source and compiled at run time
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#include <metal_stdlib>
using namespace metal;

// A solid-colour pass: positions in clip space, one colour for the draw.
// The band draws with its own shaders, NereusKit/Sources/NereusBand/Shaders.

struct SolidVertexOut {
    float4 position [[position]];
};

vertex SolidVertexOut solidVertex(uint vertexId [[vertex_id]],
                                  constant float2 *positions [[buffer(0)]]) {
    SolidVertexOut out;
    out.position = float4(positions[vertexId], 0.0, 1.0);
    return out;
}

fragment float4 solidFragment(SolidVertexOut in [[stage_in]],
                              constant float4 &colour [[buffer(0)]]) {
    return colour;
}
