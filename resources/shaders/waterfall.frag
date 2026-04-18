#version 440

// =================================================================
// resources/shaders/waterfall.frag  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (ten9876/AetherSDR, GPLv3) shader source:
//   resources/shaders/texturedquad.frag — ring-buffer UV-offset
//   sampling pattern (line 19 below). AetherSDR has no per-file
//   shader headers; project-level citation per
//   docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in GLSL 440 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code. The texture sampler binding, std140
//                 uniform layout, and rowOffset uniform are NereusSDR
//                 additions on top of AetherSDR's UV-fract pattern.
// =================================================================

layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 fragColor;

layout(binding = 1) uniform sampler2D tex;

layout(std140, binding = 0) uniform Uniforms {
    float rowOffset;
    float padding1;
    float padding2;
    float padding3;
};

void main()
{
    // Apply ring buffer offset per-pixel (not per-vertex, to avoid fract() interpolation issue)
    // From AetherSDR texturedquad.frag
    vec2 uv = vec2(v_uv.x, fract(v_uv.y + rowOffset));
    fragColor = texture(tex, uv);
}
