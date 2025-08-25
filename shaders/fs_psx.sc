$input v_texcoord0
$output

#include <bgfx_shader.sh>

SAMPLER2D(s_albedo, 0);
uniform vec4 u_ColorTint;
uniform vec4 u_psxParams; // x=jitter_amp_px, y=affine_factor

void main()
{
    // Affine warp by pushing UVs toward their triangle average; we don't have barycentrics here,
    // so approximate by quantizing UVs which induces affine-looking swim when animated.
    float aff = clamp(u_psxParams.y, 0.0, 1.0);
    vec2 uv = v_texcoord0;
    float stepUV = mix(0.0, 1.0/64.0, aff);
    if (stepUV > 0.0) {
        uv = floor(uv / stepUV + 0.5) * stepUV;
    }

    vec4 c = texture2D(s_albedo, uv) * u_ColorTint;
    gl_FragColor = c;
}


