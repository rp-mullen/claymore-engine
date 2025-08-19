$input a_position
$output v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    gl_Position = vec4(a_position, 1.0);
    // Map clip-space verts to UV for gradient generation
    v_texcoord0.xy = a_position.xy * 0.5 + 0.5;
}


