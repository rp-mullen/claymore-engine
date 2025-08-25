$input a_position, a_normal, a_texcoord0
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_psxParams; // x=jitter_amp_px, y=affine_factor [0..1], z=unused, w=unused

void main()
{
    vec4 wp = mul(u_model[0], vec4(a_position, 1.0));
    // Apply simple screen-space jitter before projection: quantize NDC to emulate PSX wobble
    vec4 clip = mul(u_viewProj, wp);
    float px = max(u_psxParams.x, 0.0); // pixels
    if (px > 0.0) {
        // project to ndc
        vec2 ndc = clip.xy / max(clip.w, 1e-6);
        // assume 1080p-ish scaling; bgfx doesn't expose resolution here, so leave in NDC units via a heuristic
        float step = max(px / 540.0, 1e-6);
        ndc = floor(ndc / step + 0.5) * step;
        clip.xy = ndc * clip.w;
    }

    v_texcoord0.xy = a_texcoord0.xy;
    gl_Position = clip;
}

