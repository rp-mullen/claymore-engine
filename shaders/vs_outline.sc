$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // Fullscreen clip-space geometry supplied already in clip coords
    gl_Position = vec4(a_position, 1.0);
    v_texcoord0.xy = a_position.xy * 0.5 + 0.5;
}


