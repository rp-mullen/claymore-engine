$input a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_worldPos, v_normal, v_texcoord0, v_viewDir

#include <bgfx_shader.sh>

uniform vec4 u_cameraPos;
uniform mat4 u_bones[64];

void main()
{
    ivec4 idx = ivec4(a_indices);
    vec4  w   = a_weight;
    // Ensure weights are normalized to avoid scale/shear from cumulative error
    float sumW = w.x + w.y + w.z + w.w;
    if (sumW > 0.0001) {
        w /= sumW;
    }

    mat4 skin = w.x * u_bones[idx.x] +
                w.y * u_bones[idx.y] +
                w.z * u_bones[idx.z] +
                w.w * u_bones[idx.w];

    vec4 skinnedPos   = mul(vec4(a_position, 1.0), skin);
    // Skin normal using skin matrix. Using mat3 from mat4 to avoid translation component.
    vec3 skinnedNormal = mul(vec4(a_normal, 0.0), skin).xyz;

    vec4 worldPos = mul(u_model[0], skinnedPos);

    v_worldPos = worldPos.xyz;
    v_normal   = normalize( mul(vec4(skinnedNormal,0.0), u_model[0]).xyz );
    v_texcoord0.xy = a_texcoord0.xy;
    v_viewDir  = normalize(u_cameraPos.xyz - worldPos.xyz);

    gl_Position = mul(u_viewProj, worldPos);
}
