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

// The 3D view's stacked trace, oldest first so nearer rows cover farther
// ones; two vertices per column, the ridge's top and the foot of the row in
// front. Each row keeps its own window, so a column lands where its
// frequency now falls in the view.
//
// Every row but the newest glides back from the front as the next row
// comes: the row `age` rows old (1 or more) sits (age - 1 + glide) rows
// deep. The front stays put. It shows the row one older than the newest
// in full, and over it the newest row grows from clear to whole as the
// glide runs, so when the next row arrives the front already shows it and
// nothing jumps. The instances: the gliding rows, then the front's two
// layers, the older below and the newest over it.

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
    float opacity;        // 1, or the newest row's share while it grows in at the front
};

// NereusSDR modification history: 2026-10-12 J.J. Boyd (KG4VCF),
// AI-assisted via OpenAI Codex. Accepted call56: retained measured quiet
// only outside viewport/captured coverage; no simulated texture or DSP.
// Host rows RG=(measured dBm,captured coverage), window RGBA=(low,span,quiet,known).
float2 stackedSample(float unit, float4 window, uint row, constant StackedUniforms &u,
                     texture2d<float, access::read> rows) {
    float hz = u.view.x + unit * u.view.y;
    float position = window.y > 0.0 ? (hz - window.x) / window.y : -1.0;
    if (position >= 0.0 && position < 1.0) {
        uint column = min(uint(position * float(u.columns)), u.columns - 1);
        float2 measured = rows.read(uint2(column, row)).rg;
        if (measured.y > 0.5) {
            return float2(measured.x, isfinite(measured.x) ? 1.0 : 0.0);
        }
    }
    if ((unit < 0.0 || unit > 1.0) && window.w > 0.5 && isfinite(window.z)) {
        return float2(window.z, 1.0);
    }
    return float2(u.levels.x, 0.0);
}

vertex StackedOut bandStackedVertex(uint vertexId [[vertex_id]], uint instance [[instance_id]],
                                    constant StackedUniforms &u [[buffer(0)]],
                                    texture2d<float, access::read> rows [[texture(0)]],
                                    texture2d<float, access::read> windows [[texture(1)]]) {
    StackedOut out;
    uint shown = min(u.count, u.capacity);
    float glide = u.view.z;
    // The gliding rows, oldest first (shown - 1 of them), then the front:
    // the row before the newest, whole, and the newest growing in.
    uint gliding = shown - 1;
    bool front = instance >= gliding;
    uint age = front ? (instance == gliding ? 1u : 0u) : shown - 1 - instance;
    float opacity = front && age == 0 ? glide : 1.0;
    uint row = (u.newestRow + u.capacity - age) % u.capacity;
    uint column = min(vertexId / 2, u.columns - 1);
    bool top = (vertexId & 1) == 0;
    float depth = front ? 0.0 : (float(age) - 1.0 + glide) / u.view.w;
    float4 window = windows.read(uint2(0, row));
    // Emit the current view's mesh and sample the retained frequency window,
    // so a source/view change keeps real channels and known overhang aligned.
    float unit = 0.5 + ((float(column) + 0.5) / float(u.columns) - 0.5) * u.look.w;
    float2 sampled = stackedSample(unit, window, row, u, rows);
    float dbm = sampled.x;
    float d = clamp(depth, 0.0, 1.0);
    float widthScale = 1.0 - d * (1.0 - u.shape.x);
    bool ok = age < shown && sampled.y > 0.5 && isfinite(dbm) && window.y > 0.0 && depth <= 1.0;
    float share = ok ? pow(clamp((dbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w) : 0.0;
    float baseY = u.plot.y + u.plot.w * (1.0 - d * u.shape.y);
    float ridgeY = baseY - share * u.shape.z * widthScale * u.plot.w;
    // The ridge's slope from its neighbours, so the line is as wide across
    // a steep flank as along a flat top (the row drawn as a stroked line).
    uint left = column > 0 ? column - 1 : column;
    uint right = min(column + 1, u.columns - 1);
    float leftUnit = 0.5 + ((float(left) + 0.5) / float(u.columns) - 0.5) * u.look.w;
    float rightUnit = 0.5 + ((float(right) + 0.5) / float(u.columns) - 0.5) * u.look.w;
    float leftDbm = stackedSample(leftUnit, window, row, u, rows).x;
    float rightDbm = stackedSample(rightUnit, window, row, u, rows).x;
    leftDbm = isfinite(leftDbm) ? leftDbm : dbm;
    rightDbm = isfinite(rightDbm) ? rightDbm : dbm;
    float rise = (pow(clamp((rightDbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w)
                  - pow(clamp((leftDbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w))
                 * u.shape.z * widthScale * u.plot.w;
    float run = max(float(right - left), 1.0) / float(u.columns) * u.look.w * u.plot.z * widthScale;
    float slope = ok ? rise / max(run, 0.001) : 0.0;
    // The front reaches the plot's floor; a gliding row stands on the row
    // in front of it, one row nearer (the front itself for the nearest).
    float footY = u.plot.y + u.plot.w;
    if (!front) {
        float nearer = clamp((float(age) - 2.0 + glide) / u.view.w, 0.0, 1.0);
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
    out.opacity = ok ? opacity : 0.0;
    return out;
}

// Slice Shadow: each passband darkens the surface, with its centre cued in the slice's colour.
static float3 stackedShadow(float3 colour, float unit, float widthScale, constant StackedUniforms &u) {
    uint passbands = min(uint(u.shadowMeta.x), 8u);
    float unitsPerPixel = 1.0 / max(u.plot.z * widthScale, 1.0);
    for (uint i = 0; i < passbands; i++) {
        float4 band = u.shadows[i];
        float3 cue = u.cues[i].rgb;
        float3 dark = float3(0.008) + cue * 0.045;
        if (unit >= band.x && unit <= band.y) {
            colour = mix(colour, dark, u.shadowMeta.y);
        }
        if (abs(unit - band.z) <= u.shadowMeta.w * unitsPerPixel) {
            colour = mix(colour, cue, u.shadowMeta.z);
        }
    }
    return colour;
}

fragment float4 bandStackedFragment(StackedOut in [[stage_in]],
                                    constant StackedUniforms &u [[buffer(0)]],
                                    texture2d<float> palette [[texture(0)]]) {
    if (in.valid < 0.999 || in.opacity <= 0.0 || abs(in.unit - 0.5) > u.look.w * 0.5) {
        discard_fragment();
    }
    constexpr sampler lookup(filter::nearest, address::clamp_to_edge);
    float3 lit = palette.sample(lookup, float2((clamp(in.colour, 0.0, 1.0) * 255.0 + 0.5) / 256.0, 0.5)).rgb;
    float3 background = u.background.rgb;
    // The ridge line in the whole colour, the body under it dimmer.
    float3 colour = mix(background, lit, in.below * in.across < u.look.z ? 1.0 : u.look.y);
    colour = stackedShadow(colour, in.unit, in.widthScale, u);
    // Farther rows fade toward the background.
    colour = mix(colour, background, in.depth * u.look.x);
    // Premultiplied: the newest row at the front lets the older show through.
    float opacity = clamp(in.opacity, 0.0, 1.0);
    return float4(colour * opacity, opacity);
}

// The 3D view's ridge lines: a light line along every row's ridge, drawn
// over the whole stack. The lines do not glide with the rows. Each stays
// at its own depth, one a row from the front back, and there shows the two
// rows the stack is gliding between: the older fading out as the newer
// fades in, so the lines dissolve from one row's shape to the next rather
// than stepping back. Each line is a ribbon a line's width across its
// ridge with soft edges, two vertices per column; the instances run from
// the back, the older layer of each depth before the newer.

struct StackedLineOut {
    float4 position [[position]];
    float side;           // -1 to 1 across the ribbon
    float depth;
    float unit;
    float strength;       // the level's share of the colour span, 0 to 1
    float layer;          // 0 the older row at this depth, 1 the newer
    float valid;
    float widthScale;
};

static float2 stackedRidgePoint(float unit, float dbm, bool ok, float depth, constant StackedUniforms &u) {
    float d = clamp(depth, 0.0, 1.0);
    float widthScale = 1.0 - d * (1.0 - u.shape.x);
    float share = ok ? pow(clamp((dbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w) : 0.0;
    float baseY = u.plot.y + u.plot.w * (1.0 - d * u.shape.y);
    float x = u.plot.x + u.plot.z * (0.5 + (unit - 0.5) * widthScale);
    return float2(x, baseY - share * u.shape.z * widthScale * u.plot.w);
}

vertex StackedLineOut bandStackedLineVertex(uint vertexId [[vertex_id]], uint instance [[instance_id]],
                                            constant StackedUniforms &u [[buffer(0)]],
                                            texture2d<float, access::read> rows [[texture(0)]],
                                            texture2d<float, access::read> windows [[texture(1)]]) {
    StackedLineOut out;
    uint shown = min(u.count, u.capacity);
    uint depths = min(shown, uint(u.view.w));
    uint place = depths - 1 - min(instance / 2, depths - 1);
    bool newer = (instance & 1) == 1;
    uint age = newer ? place : place + 1;
    uint row = (u.newestRow + u.capacity - min(age, u.capacity - 1)) % u.capacity;
    uint column = min(vertexId / 2, u.columns - 1);
    float depth = float(place) / u.view.w;
    float4 window = windows.read(uint2(0, row));
    float unit = 0.5 + ((float(column) + 0.5) / float(u.columns) - 0.5) * u.look.w;
    float2 sampled = stackedSample(unit, window, row, u, rows);
    float dbm = sampled.x;
    bool ok = age < shown && sampled.y > 0.5 && isfinite(dbm) && window.y > 0.0;
    float2 point = stackedRidgePoint(unit, dbm, ok, depth, u);
    // Across the ridge from its neighbours, so a steep flank's line is as
    // wide as a flat top's.
    uint left = column > 0 ? column - 1 : column;
    uint right = min(column + 1, u.columns - 1);
    float leftUnit = 0.5 + ((float(left) + 0.5) / float(u.columns) - 0.5) * u.look.w;
    float rightUnit = 0.5 + ((float(right) + 0.5) / float(u.columns) - 0.5) * u.look.w;
    float leftDbm = stackedSample(leftUnit, window, row, u, rows).x;
    float rightDbm = stackedSample(rightUnit, window, row, u, rows).x;
    float2 along = stackedRidgePoint(rightUnit, isfinite(rightDbm) ? rightDbm : dbm, ok, depth, u)
                   - stackedRidgePoint(leftUnit, isfinite(leftDbm) ? leftDbm : dbm, ok, depth, u);
    float2 normal = length(along) > 0.0001 ? normalize(float2(-along.y, along.x)) : float2(0.0, 1.0);
    float side = (vertexId & 1) == 0 ? -1.0 : 1.0;
    float halfWidth = max(u.look.z, 1.0);
    out.position = clipPosition(point + normal * side * halfWidth, u.viewport);
    out.side = side;
    out.depth = clamp(depth, 0.0, 1.0);
    out.unit = unit;
    out.strength = ok ? clamp((dbm - u.levels.x) / u.levels.z, 0.0, 1.0) : 0.0;
    out.layer = newer ? 1.0 : 0.0;
    out.valid = ok ? 1.0 : 0.0;
    out.widthScale = 1.0 - out.depth * (1.0 - u.shape.x);
    return out;
}

fragment float4 bandStackedLineFragment(StackedLineOut in [[stage_in]],
                                        constant StackedUniforms &u [[buffer(0)]]) {
    if (in.valid < 0.999 || abs(in.unit - 0.5) > u.look.w * 0.5) {
        discard_fragment();
    }
    float nearness = 1.0 - in.depth;
    // A light grey, brighter and stronger near the front, fainter over the
    // floor: the numbers are StackedTrace's line constants.
    float3 colour = mix(u.background.rgb, float3(0.92), 0.45 + 0.55 * nearness);
    colour = stackedShadow(colour, in.unit, in.widthScale, u);
    float overFloor = mix(0.42, 1.0, smoothstep(0.0, 0.035, in.strength));
    float edge = 1.0 - smoothstep(0.15, 1.0, abs(in.side));
    float strength = (0.12 + 0.5 * nearness) * overFloor * edge;
    // The older layer fades out with the glide. The newer, drawn over it,
    // is strengthened so a line both rows share keeps its strength.
    float glide = clamp(u.view.z, 0.0, 1.0);
    float older = strength * (1.0 - glide);
    float alpha = in.layer > 0.5 ? strength * glide / max(1.0 - older, 0.0001) : older;
    if (alpha <= 0.0) {
        discard_fragment();
    }
    return float4(colour * alpha, alpha);
}
