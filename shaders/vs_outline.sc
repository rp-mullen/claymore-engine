$input a_position
$output v_color0

#include <bgfx_shader.sh>

uniform vec4 u_outlineColor;

void main()
{
    v_color0 = u_outlineColor;
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}


