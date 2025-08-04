$input v_worldPos, v_normal, v_uv0, v_viewDir

#include <bgfx_shader.sh>

SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_metallicRoughness, 1);
SAMPLER2D(s_normalMap, 2);

uniform vec4 u_lightDir;    // xyz = direction
uniform vec4 u_lightColor;  // rgb = color

void main()
{
    vec3 N = normalize(v_normal);
    vec3 V = normalize(v_viewDir);
    vec3 L = normalize(-u_lightDir.xyz);
    vec3 H = normalize(V + L);

    // Sample material properties
    vec3 baseColor = texture2D(s_albedo, v_uv0).rgb; 
    float metallic = texture2D(s_metallicRoughness, v_uv0).r;
    float roughness = texture2D(s_metallicRoughness, v_uv0).g;

    // Fresnel-Schlick
    vec3 F0 = mix(vec3(0.04,0.04,0.04), baseColor, metallic);
    float VdotH = max(dot(V, H), 0.0);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // Normal Distribution (GGX)
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float NdotH = max(dot(N, H), 0.0);
    float denom = (NdotH * NdotH) * (alpha2 - 1.0) + 1.0;
    float D = alpha2 / (3.14159 * denom * denom);

    // Geometry (Smith-Schlick)
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G_V = NdotV / (NdotV * (1.0 - k) + k);
    float G_L = NdotL / (NdotL * (1.0 - k) + k);
    float G = G_V * G_L;

    // Cook-Torrance specular
    vec3 numerator = D * F * G;
    float denominator = 4.0 * NdotV * NdotL + 0.001;
    vec3 specular = numerator / denominator;

    // Diffuse (Lambert)
    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    vec3 diffuse = baseColor / 3.14159;

    vec3 color = (kD * diffuse + specular) * NdotL * u_lightColor.rgb;

    gl_FragColor = vec4(color, 1.0);
}
