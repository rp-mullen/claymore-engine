$input v_texcoord0

#include <bgfx_shader.sh>

uniform vec4 u_skyParams;   // x = procedural sky enabled
uniform vec4 u_skyZenith;   // rgb = zenith color
uniform vec4 u_skyHorizon;  // rgb = horizon color

void main()
{
    if (u_skyParams.x < 0.5) { discard; }
    float t = clamp(1.0 - v_texcoord0.y, 0.0, 1.0);
    vec3 color = mix(u_skyZenith.rgb, u_skyHorizon.rgb, t);
    gl_FragColor = vec4(color, 1.0);
}


