$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(sEdgeMask,   0);

uniform vec4 uTexelSize;    // (1/w,1/h,0,0)
uniform vec4 uColor;        // outline color RGBA

void main()
{
    vec2 uv = v_texcoord0.xy;
    float m = texture2D(sEdgeMask, uv).r;
    float a = clamp(m * uColor.a, 0.0, 1.0);
    gl_FragColor = vec4(uColor.rgb, a);
}
