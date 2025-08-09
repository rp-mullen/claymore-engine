$input a_position
$output v_color0

#include <bgfx_shader.sh>

void main()
{
    // Semi-transparent gray
    v_color0 = vec4(0.5, 0.5, 0.5, 0.35);
    gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0));
}
