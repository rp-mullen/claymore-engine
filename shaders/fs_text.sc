$input v_texcoord0, v_color0

#include <bgfx_shader.sh>

SAMPLER2D(s_text, 0);

void main()
{
    // Sample R8 distance/coverage; use as alpha
    float a = texture2D(s_text, v_texcoord0.xy).r;
    gl_FragColor = vec4(v_color0.rgb, v_color0.a * a);
}


