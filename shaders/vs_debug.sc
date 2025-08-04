$input a_position
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    v_color0 = vec4(1.0, 1.0, 1.0, 1.0); // White lines
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
