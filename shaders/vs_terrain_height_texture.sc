$input a_position, a_texcoord0
$output v_position, v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(s_heightTexture, 0);

void main()
{
    v_texcoord0 = a_texcoord0;
    vec3 pos = a_position;
    float height = texture2DLod(s_heightTexture, a_texcoord0, 0.0).x * 255.0;
    pos.y = height;
    v_position = pos;

    gl_Position = mul(u_modelViewProj, vec4(pos, 1.0));
}