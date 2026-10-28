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

// The waterfall: original stored rows selected by the logical-time axis, each a
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
                                      device const uint *rowAges [[buffer(1)]],
                                      texture2d<float, access::read> lines [[texture(0)]],
                                      texture2d<float> palette [[texture(1)]],
                                      texture2d<float, access::read> coverages [[texture(2)]]) {
    uint age = rowAges[uint(max(0.0, floor(in.position.y - uniforms.rect.y)))];
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
// ones; two vertices per column, the ridge's top and the plot's floor, so
// each row hangs a curtain to the floor. Each row keeps its own window, so
// a column lands where its frequency now falls in the view.
//
// The rows between the front and the back glide back as the next row
// comes: the row `age` rows old sits (age - 1 + glide) rows deep. The
// front stays put. It shows the row one older than the newest in full,
// and over it the newest row grows from clear to whole as the glide runs,
// so when the next row arrives the front already shows it and nothing
// jumps. The back stays put the same way, one row short of the deepest:
// the oldest row whole, the one after it growing in. The instances, from
// the back: the back's two layers, the gliding rows, the front's two
// layers; the older layer of each pair below the newer.

struct StackedUniforms {
    float4 plot;          // the spectrum's plot: x, y, width, height in pixels
    float4 background;
    float4 shadows[8];    // each passband's low, high and centre, in the view's units
    float4 cues[8];       // each passband's colour
    float4 shape;         // back width, depth span, ridge, height curve
    float4 levels;        // floor dBm, height range dB, colour range dB, gamma
    float4 look;          // haze, the curtain's colour share at the floor, ridge line half width in pixels, row span
    float4 view;          // the view's low edge from the rows' reference and its span (Hz), unused, visible rows
    float4 shadowMeta;    // passbands, shadow strength, cue strength, cue half width in pixels
    float4 glides;        // the rows' glide, the front's newest row opacity, the back's next row opacity, the back's depth
    float4 line;          // the ridge line's grey, its share over the background at the back, its strength at the back, its extra strength at the front
    float4 lineFloor;     // the line's share over the floor, the floor's span, the rows' side fade in pixels, where a line's edge starts to soften
    float2 viewport;
    uint columns;
    uint capacity;
    uint count;
    uint newestRow;
    uint meshColumns;     // the mesh's columns across the row span
};

struct StackedOut {
    float4 position [[position]];
    float colour;         // the level's share of the colour span, less toward the floor
    float depth;
    float unit;
    float valid;
    float widthScale;
    float end;            // pixels in from the row's nearer end
    float opacity;        // 1, or a growing row's share at the front or the back
};

// How far in from the row's nearer end a frequency lands, in pixels.
static float stackedEnd(float unit, float widthScale, constant StackedUniforms &u) {
    return max(u.look.w * 0.5 - abs(unit - 0.5), 0.0) * widthScale * u.plot.z;
}

// The rows' ends fade over StackedTrace.sideFadePixels, which the renderer passes in.
static float stackedSideFade(float end, constant StackedUniforms &u) {
    return smoothstep(0.0, u.lineFloor.z, end);
}

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
    // Every depth and opacity below is StackedTraceGeometry's, passed in.
    float glide = u.glides.x;
    uint deepest = uint(u.view.w);
    // From the back: the oldest row and the one after it growing in, both
    // at the back's depth; the gliding rows, oldest first; then the front:
    // the row before the newest, whole, and the newest growing in.
    uint age;
    float depth;
    float opacity = 1.0;
    if (instance < 2) {
        age = deepest - instance;
        depth = u.glides.w;
        opacity = instance == 1 ? u.glides.z : 1.0;
    } else if (instance < deepest) {
        age = deepest - instance;
        depth = (float(age) - 1.0 + glide) / u.view.w;
    } else {
        age = instance == deepest ? 1u : 0u;
        depth = 0.0;
        opacity = age == 0 ? u.glides.y : 1.0;
    }
    uint row = (u.newestRow + u.capacity - min(age, u.capacity - 1)) % u.capacity;
    uint column = min(vertexId / 2, u.meshColumns - 1);
    bool top = (vertexId & 1) == 0;
    float4 window = windows.read(uint2(0, row));
    // Emit the current view's mesh and sample the retained frequency window,
    // so a source/view change keeps real channels and known overhang aligned.
    float unit = 0.5 + ((float(column) + 0.5) / float(u.meshColumns) - 0.5) * u.look.w;
    float2 sampled = stackedSample(unit, window, row, u, rows);
    float dbm = sampled.x;
    float d = clamp(depth, 0.0, 1.0);
    float widthScale = 1.0 - d * (1.0 - u.shape.x);
    bool ok = age < shown && sampled.y > 0.5 && isfinite(dbm) && window.y > 0.0 && depth <= 1.0;
    float share = ok ? pow(clamp((dbm - u.levels.x) / u.levels.y, 0.0, 1.0), u.shape.w) : 0.0;
    float baseY = u.plot.y + u.plot.w * (1.0 - d * u.shape.y);
    float ridgeY = baseY - share * u.shape.z * widthScale * u.plot.w;
    // Every row's curtain hangs to the plot's floor; the nearer rows,
    // drawn after, cover what lies behind them.
    float footY = max(u.plot.y + u.plot.w, ridgeY);
    float x = u.plot.x + u.plot.z * (0.5 + (unit - 0.5) * widthScale);
    out.position = clipPosition(float2(x, top ? ridgeY : footY), u.viewport);
    // The colour share at the ridge, falling to the floor's share of it.
    float colour = ok ? clamp((dbm - u.levels.x) / u.levels.z, 0.0, 1.0) : 0.0;
    out.colour = top ? colour : colour * u.look.y;
    out.depth = d;
    out.unit = unit;
    out.valid = ok ? 1.0 : 0.0;
    out.widthScale = widthScale;
    out.end = stackedEnd(unit, widthScale, u);
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
    // 3D Gain's curve on the curtain's colour share, then the palette.
    float position = pow(clamp(in.colour, 0.0, 1.0), u.levels.w);
    float3 colour = palette.sample(lookup, float2((position * 255.0 + 0.5) / 256.0, 0.5)).rgb;
    // Farther rows fade toward the background.
    colour = mix(colour, u.background.rgb, clamp(in.depth * u.look.x, 0.0, 1.0));
    colour = stackedShadow(colour, in.unit, in.widthScale, u);
    // Premultiplied: a growing row lets the older show through, and the
    // row's ends fade out.
    float opacity = clamp(in.opacity, 0.0, 1.0) * stackedSideFade(in.end, u);
    return float4(colour * opacity, opacity);
}

// The 3D view's ridge lines: a light line along every row's ridge, drawn
// over the whole stack. The lines do not glide with the rows. Each stays
// at its own depth, one a row from the front back, and there shows the two
// rows the stack is gliding between: the older fading out as the newer
// fades in, so the lines dissolve from one row's shape to the next rather
// than stepping back. Each line is a strip a line's width across its
// ridge with soft edges, two vertices per column; the instances run from
// the back, the older layer of each depth before the newer.

struct StackedLineOut {
    float4 position [[position]];
    float side;           // -1 to 1 across the strip
    float depth;
    float unit;
    float strength;       // the level's share of the colour span, 0 to 1
    float layer;          // 0 the older row at this depth, 1 the newer
    float valid;
    float widthScale;
    float end;            // pixels in from the row's nearer end
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
    uint column = min(vertexId / 2, u.meshColumns - 1);
    float depth = float(place) / u.view.w;
    float4 window = windows.read(uint2(0, row));
    float unit = 0.5 + ((float(column) + 0.5) / float(u.meshColumns) - 0.5) * u.look.w;
    float2 sampled = stackedSample(unit, window, row, u, rows);
    float dbm = sampled.x;
    bool ok = age < shown && sampled.y > 0.5 && isfinite(dbm) && window.y > 0.0;
    float2 point = stackedRidgePoint(unit, dbm, ok, depth, u);
    // Across the ridge from its neighbours, so a steep flank's line is as
    // wide as a flat top's.
    uint left = column > 0 ? column - 1 : column;
    uint right = min(column + 1, u.meshColumns - 1);
    float leftUnit = 0.5 + ((float(left) + 0.5) / float(u.meshColumns) - 0.5) * u.look.w;
    float rightUnit = 0.5 + ((float(right) + 0.5) / float(u.meshColumns) - 0.5) * u.look.w;
    float leftDbm = stackedSample(leftUnit, window, row, u, rows).x;
    float rightDbm = stackedSample(rightUnit, window, row, u, rows).x;
    float2 along = stackedRidgePoint(rightUnit, isfinite(rightDbm) ? rightDbm : dbm, ok, depth, u)
                   - stackedRidgePoint(leftUnit, isfinite(leftDbm) ? leftDbm : dbm, ok, depth, u);
    // The strip's sides lie square to the ridge; where the neighbours give
    // the ridge no run at all, the strip stands upright.
    float2 across = float2(-along.y, along.x);
    float run = dot(across, across);
    float2 normal = run > 1.0e-8 ? across * rsqrt(run) : float2(0.0, 1.0);
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
    out.end = stackedEnd(unit, out.widthScale, u);
    return out;
}

fragment float4 bandStackedLineFragment(StackedLineOut in [[stage_in]],
                                        constant StackedUniforms &u [[buffer(0)]]) {
    if (in.valid < 0.999 || abs(in.unit - 0.5) > u.look.w * 0.5) {
        discard_fragment();
    }
    float nearness = 1.0 - in.depth;
    // A light grey, brighter and stronger near the front, fainter over the
    // floor: StackedTrace's line constants, which the renderer passes in.
    float3 colour = mix(u.background.rgb, float3(u.line.x), u.line.y + (1.0 - u.line.y) * nearness);
    colour = stackedShadow(colour, in.unit, in.widthScale, u);
    float overFloor = mix(u.lineFloor.x, 1.0, smoothstep(0.0, u.lineFloor.y, in.strength));
    float edge = 1.0 - smoothstep(u.lineFloor.w, 1.0, abs(in.side));
    float strength = (u.line.z + u.line.w * nearness) * overFloor * edge * stackedSideFade(in.end, u);
    // Two layers share each depth: the older row's line, drawn first, gives
    // way as the glide runs, and the newer row's goes over it. Where both
    // rows draw the same line the pair should cover what one line covers,
    // `strength`: over the older's share the newer covers the part of what
    // is left that makes up the difference.
    float glide = clamp(u.glides.x, 0.0, 1.0);
    float older = strength * (1.0 - glide);
    float left = 1.0 - older;
    float alpha = in.layer > 0.5 ? (left > 0.0 ? (strength - older) / left : 0.0) : older;
    if (alpha <= 0.0) {
        discard_fragment();
    }
    return float4(colour * alpha, alpha);
}
