$input  a_position, a_normal, a_texcoord0, a_indices, a_weight
$output v_worldPos, v_normal, v_texcoord0, v_viewDir

#include <bgfx_shader.sh>

uniform vec4 u_cameraPos;
uniform mat4 u_bones[128];
uniform mat4 u_normalMat; // provided by CPU (transpose(inverse(mat3(model))))

void main()
{
    // Bone indices & weights
    ivec4 idx = ivec4(a_indices);
    vec4  w   = a_weight;

    // Keep weights sane; avoid scaling/shear from drift.
    float sumW = w.x + w.y + w.z + w.w;
    if (sumW > 0.0) {
        w /= sumW;
    }

    // Skinning matrix (linear blend)
    mat4 skin =
          w.x * u_bones[idx.x]
        + w.y * u_bones[idx.y]
        + w.z * u_bones[idx.z]
        + w.w * u_bones[idx.w];

    // Model * Skin once
    mat4 skinModel = mul(u_model[0], skin);

    // World position
    vec4 worldPos  = mul(skinModel, vec4(a_position, 1.0));
    v_worldPos     = worldPos.xyz;

    // Skinned normal (ignore translation)
    vec3 skinnedNormal = mul(skin, vec4(a_normal, 0.0)).xyz;

    // Transform to world using CPU-provided normal matrix and normalize
    vec3 worldNormal = normalize(mul((mat3)u_normalMat, skinnedNormal));
    v_normal         = worldNormal;

    // Varyings
    v_texcoord0 = a_texcoord0;
    v_viewDir   = normalize(u_cameraPos.xyz - worldPos.xyz);

    // Clip-space
    gl_Position = mul(u_viewProj, worldPos);
}
