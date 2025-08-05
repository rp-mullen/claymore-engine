$input v_position, v_texcoord0

#include <bgfx_shader.sh>

void main()
{
    // Simple lambert lighting based on normal up vector
    vec3 lightDir = normalize(vec3(0.3, 1.0, 0.2));
    float ndotl = clamp(dot(vec3(0.0, 1.0, 0.0), lightDir), 0.0, 1.0);
    vec3 baseColor = vec3(0.2, 0.7, 0.2);
    vec3 color = baseColor * (0.3 + 0.7 * ndotl);
    gl_FragColor = vec4(color, 1.0);
}