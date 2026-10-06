// NereusSDR for iOS: the band's Metal shaders, shipped as source and compiled at run time
// SPDX-License-Identifier: GPL-3.0-or-later WITH AdditionRef-NereusSDR-AppStore-permission

#include <metal_stdlib>
using namespace metal;

// Every position is in the target's pixels, from its top-left corner.

struct BandViewport {
    float2 size;
};

static float4 clipPosition(float2 pixel, float2 size) {
    return float4(pixel.x / size.x * 2.0 - 1.0, 1.0 - pixel.y / size.y * 2.0, 0.0, 1.0);
}

// Coloured triangles: the grid, the trace and its fill, the extras, the strip.

struct ColourVertex {
    float2 position;
    float4 colour;
};

struct ColourOut {
    float4 position [[position]];
    float4 colour;
};

vertex ColourOut bandColourVertex(uint vertexId [[vertex_id]],
                                  const device ColourVertex *vertices [[buffer(0)]],
                                  constant BandViewport &viewport [[buffer(1)]]) {
    ColourOut out;
    out.position = clipPosition(vertices[vertexId].position, viewport.size);
    out.colour = vertices[vertexId].colour;
    return out;
}

fragment float4 bandColourFragment(ColourOut in [[stage_in]]) {
    // Premultiplied, for the one blend every pass uses.
    return float4(in.colour.rgb * in.colour.a, in.colour.a);
}

// A textured rectangle: the labels, drawn by Core Graphics, premultiplied.

struct RectUniforms {
    float4 rect;      // x, y, width, height in pixels
    float2 viewport;
};

struct RectOut {
    float4 position [[position]];
    float2 uv;
};

vertex RectOut bandRectVertex(uint vertexId [[vertex_id]],
                              constant RectUniforms &uniforms [[buffer(0)]]) {
    float2 corner = float2(float(vertexId & 1), float((vertexId >> 1) & 1));
    RectOut out;
    float2 pixel = uniforms.rect.xy + corner * uniforms.rect.zw;
    out.position = clipPosition(pixel, uniforms.viewport);
    out.uv = corner;
    return out;
}

fragment float4 bandLabelFragment(RectOut in [[stage_in]],
                                  texture2d<float> labels [[texture(0)]]) {
    constexpr sampler nearest(filter::nearest, address::clamp_to_edge);
    return labels.sample(nearest, in.uv);
}

// The waterfall: one stored line per pixel row, newest at the top, each a
// palette position the history coloured when the line arrived.

struct WaterfallUniforms {
    float4 rect;         // x, y, width, height in pixels
    uint columns;
    uint capacity;
    uint count;
    uint nextRow;        // the storage row the next line goes to
    float4 background;
    float4 view;         // the view's low edge from the rows' reference, its span (Hz); z the lines looked back; w 1 when rows carry coverage
};

fragment float4 bandWaterfallFragment(RectOut in [[stage_in]],
                                      constant WaterfallUniforms &uniforms [[buffer(0)]],
                                      texture2d<float, access::read> lines [[texture(0)]],
                                      texture2d<float> palette [[texture(1)]],
                                      texture2d<float, access::read> coverages [[texture(2)]]) {
    uint age = uint(max(0.0, floor(in.position.y - uniforms.rect.y))) + uint(uniforms.view.z);
    if (uniforms.count == 0 || uniforms.columns == 0 || age >= uniforms.count) {
        return float4(uniforms.background.rgb * uniforms.background.a, uniforms.background.a);
    }
    uint row = (uniforms.nextRow + uniforms.capacity - 1 - age) % uniforms.capacity;
    float across = (in.position.x - uniforms.rect.x) / uniforms.rect.z;
    // A row covering other frequencies than the view (the view moved ahead
    // of the Core) is drawn where its own frequencies fall; past its edges
    // the view is empty.
    if (uniforms.view.w > 0.5) {
        float2 coverage = coverages.read(uint2(0, row)).rg;
        if (coverage.y > 0.0) {
            float hz = uniforms.view.x + across * uniforms.view.y;
            across = (hz - coverage.x) / coverage.y;
            if (across < 0.0 || across >= 1.0) {
                return float4(uniforms.background.rgb * uniforms.background.a, uniforms.background.a);
            }
        }
    }
    uint column = min(uint(max(0.0, floor(across * float(uniforms.columns)))), uniforms.columns - 1);
    float position = lines.read(uint2(column, row)).r;
    constexpr sampler lookup(filter::nearest, address::clamp_to_edge);
    float4 colour = palette.sample(lookup, float2((position * 255.0 + 0.5) / 256.0, 0.5));
    return float4(colour.rgb, 1.0);
}

// The 3D view's stacked trace: one instance per stored row, oldest first so
// nearer rows cover farther ones; two vertices per column, the ridge's top
// and the foot of the row in front. Each row keeps its own window, so a
// column lands where its frequency now falls in the view.

struct StackedUniforms {
    float4 plot;          // the spectrum's plot: x, y, width, height in pixels
    float4 background;
    float4 shadows[8];    // each passband's low, high and centre, in the view's units
    float4 cues[8];       // each passband's colour
    float4 shape;         // back width, depth span, ridge, height curve
    float4 levels;        // floor dBm, height range dB, colour range dB, gamma
    float4 look;          // haze, fill share, ridge line width in pixels, row span
    float4 view;          // the view's low edge from the rows' reference and its span (Hz), glide, visible rows
    float4 shadowMeta;    // passbands, shadow strength, cue strength, cue half width in pixels
    float2 viewport;
    uint columns;
    uint capacity;
    uint count;
    uint newestRow;
};

struct StackedOut {
    float4 position [[position]];
    float colour;
    float depth;
    float unit;
    float below;
    float valid;
    float widthScale;
    float across;         // the ridge's steepness: vertical distance to distance across the ridge line
};

vertex StackedOut bandStackedVertex(uint vertexId [[vertex_id]], uint instance [[instance_id]],
                                    constant StackedUniforms &u [[buffer(0)]],
                                    texture2d<float, access::read> rows [[texture(0)]],
                                    texture2d<float, access::read> windows [[texture(1)]]) {
    StackedOut out;
    uint shown = min(u.count, u.capacity);
    uint age = shown - 1 - min(instance, shown - 1);
    uint row = (u.newestRow + u.capacity - age) % u.capacity;
    uint column = min(vertexId / 2, u.columns - 1);
    bool top = (vertexId & 1) == 0;
    float glide = u.view.z;
    float depth = (float(age) + glide) / u.view.w;
    float2 window = windows.read(uint2(0, row)).rg;
    float dbm = rows.read(uint2(column, row)).r;
    float hz = window.x + (float(column) + 0.5) / float(u.columns) * window.y;
    float unit = (hz - u.view.x) / u.view.y;
    float d = clamp(depth, 0.0, 1.0);
    float widthScale = 1.0 - d * (1.0 - u.shape.x);
    bool ok = isfinite(dbm) && window.y > 0.0 && depth <= 1.0;
    float share = ok ? pow(clamp((dbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w) : 0.0;
    float baseY = u.plot.y + u.plot.w * (1.0 - d * u.shape.y);
    float ridgeY = baseY - share * u.shape.z * widthScale * u.plot.w;
    // The ridge's slope from its neighbours, so the line is as wide across
    // a steep flank as along a flat top (the row drawn as a stroked line).
    uint left = column > 0 ? column - 1 : column;
    uint right = min(column + 1, u.columns - 1);
    float leftDbm = rows.read(uint2(left, row)).r;
    float rightDbm = rows.read(uint2(right, row)).r;
    leftDbm = isfinite(leftDbm) ? leftDbm : dbm;
    rightDbm = isfinite(rightDbm) ? rightDbm : dbm;
    float rise = (pow(clamp((rightDbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w)
                  - pow(clamp((leftDbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w))
                 * u.shape.z * widthScale * u.plot.w;
    float run = max(float(right - left), 1.0) / float(u.columns) * window.y / u.view.y * u.plot.z * widthScale;
    float slope = ok ? rise / max(run, 0.001) : 0.0;
    float footY = u.plot.y + u.plot.w;
    if (age > 0) {
        float nearer = clamp((float(age) - 1.0 + glide) / u.view.w, 0.0, 1.0);
        footY = u.plot.y + u.plot.w * (1.0 - nearer * u.shape.y);
    }
    // A pixel past the row in front, so no seam shows between them.
    footY = max(footY + 1.0, ridgeY);
    float x = u.plot.x + u.plot.z * (0.5 + (unit - 0.5) * widthScale);
    out.position = clipPosition(float2(x, top ? ridgeY : footY), u.viewport);
    out.below = top ? 0.0 : footY - ridgeY;
    out.colour = ok ? pow(clamp((dbm - u.levels.x) / u.levels.z, 0.0, 1.0), u.levels.w) : 0.0;
    out.depth = d;
    out.unit = unit;
    out.valid = ok ? 1.0 : 0.0;
    out.widthScale = widthScale;
    out.across = 1.0 / sqrt(1.0 + slope * slope);
    return out;
}

fragment float4 bandStackedFragment(StackedOut in [[stage_in]],
                                    constant StackedUniforms &u [[buffer(0)]],
                                    texture2d<float> palette [[texture(0)]]) {
    if (in.valid < 0.999 || abs(in.unit - 0.5) > u.look.w * 0.5) {
        discard_fragment();
    }
    constexpr sampler lookup(filter::nearest, address::clamp_to_edge);
    float3 lit = palette.sample(lookup, float2((clamp(in.colour, 0.0, 1.0) * 255.0 + 0.5) / 256.0, 0.5)).rgb;
    float3 background = u.background.rgb;
    // The ridge line in the whole colour, the body under it dimmer.
    float3 colour = mix(background, lit, in.below * in.across < u.look.z ? 1.0 : u.look.y);
    // Slice Shadow: each passband darkens the surface, with its centre cued in the slice's colour.
    uint passbands = min(uint(u.shadowMeta.x), 8u);
    float unitsPerPixel = 1.0 / max(u.plot.z * in.widthScale, 1.0);
    for (uint i = 0; i < passbands; i++) {
        float4 band = u.shadows[i];
        float3 cue = u.cues[i].rgb;
        float3 dark = float3(0.008) + cue * 0.045;
        if (in.unit >= band.x && in.unit <= band.y) {
            colour = mix(colour, dark, u.shadowMeta.y);
        }
        if (abs(in.unit - band.z) <= u.shadowMeta.w * unitsPerPixel) {
            colour = mix(colour, cue, u.shadowMeta.z);
        }
    }
    // Farther rows fade toward the background.
    colour = mix(colour, background, in.depth * u.look.x);
    return float4(colour, 1.0);
}
