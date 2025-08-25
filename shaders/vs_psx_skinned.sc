$input  a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_psxParams; // x=jitter_amp_px, y=affine_factor
uniform mat4 u_bones[128];

void main()
{
    ivec4 idx = ivec4(a_indices);
    vec4  w   = a_weight;
    float s = max(w.x + w.y + w.z + w.w, 1e-6);
    w /= s;
    mat4 skin = w.x*u_bones[idx.x] + w.y*u_bones[idx.y] + w.z*u_bones[idx.z] + w.w*u_bones[idx.w];
    vec4 wp = mul(mul(u_model[0], skin), vec4(a_position, 1.0));
    vec4 clip = mul(u_viewProj, wp);
    float px = max(u_psxParams.x, 0.0);
    if (px > 0.0) {
        vec2 ndc = clip.xy / max(clip.w, 1e-6);
        float step = max(px / 540.0, 1e-6);
        ndc = floor(ndc / step + 0.5) * step;
        clip.xy = ndc * clip.w;
    }
    v_texcoord0.xy = a_texcoord0.xy;
    gl_Position = clip;
}

