$input a_position, a_texcoord0, a_color0
$output v_texcoord0, v_color0

#include <bgfx_shader.sh>

void main()
{
    v_texcoord0 = a_texcoord0;
    v_color0 = a_color0;
    // a_position comes from a 2D vertex buffer; missing components default to 0/1 as needed
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}


