$input a_position, a_normal, a_texcoord0
$output v_worldPos, v_normal, v_uv0, v_viewDir

#include <bgfx_shader.sh>

uniform vec4 u_cameraPos;

void main()
{
    vec4 worldPos = mul(u_model[0], vec4(a_position, 1.0)); // Object to World

    v_worldPos = worldPos.xyz;
    v_normal = normalize(mul((mat3)u_model[0], a_normal));

    v_uv0      = a_texcoord0;
    v_viewDir  = normalize(u_cameraPos.xyz - worldPos.xyz);
    gl_Position = mul(u_viewProj, worldPos);  

}
