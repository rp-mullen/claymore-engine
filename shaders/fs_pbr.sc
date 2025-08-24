$input v_worldPos, v_normal, v_texcoord0, v_viewDir

#include <bgfx_shader.sh>

SAMPLER2D(s_albedo, 0);
SAMPLER2D(s_metallicRoughness, 1);
SAMPLER2D(s_normalMap, 2);

// Light uniforms - support up to 4 lights
uniform vec4 u_lightColors[4];     // rgb = color, a = intensity
uniform vec4 u_lightPositions[4];  // xyz = position/direction, w = light type (0=directional, 1=point)
uniform vec4 u_lightParams[4];     // x = range (for point lights), y = constant, z = linear, w = quadratic
uniform vec4 u_cameraPos;          // camera position in world space
uniform vec4 u_ambientFog;         // xyz = ambient color * intensity, w = flags (bit0: fog enabled)
uniform vec4 u_fogParams;          // x = fogDensity, yzw = fog color
uniform vec4 u_skyParams;          // x = proceduralSky flag
uniform vec4 u_ColorTint;

// PBR lighting calculation function
vec3 CalculatePBRLighting(vec3 N, vec3 V, vec3 L, vec3 baseColor, float metallic, float roughness, vec3 lightColor, float lightIntensity) {
    vec3 H = normalize(V + L);
    
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

    return (kD * diffuse + specular) * NdotL * lightColor * lightIntensity;
}

void main()
{
    vec3 N = normalize(v_normal);
    vec3 V = normalize(v_viewDir);
    
    // Sample material properties
    vec3 baseColor = texture2D(s_albedo, v_texcoord0.xy).rgb; 
    baseColor *= u_ColorTint.rgb;
    float metallic = texture2D(s_metallicRoughness, v_texcoord0.xy).r;
    float roughness = texture2D(s_metallicRoughness, v_texcoord0.xy).g;

    vec3 ambientColor = u_ambientFog.xyz;
    vec3 finalColor = ambientColor; // start with ambient
    
    // Process each light
    for (int i = 0; i < 4; i++) {
        float lightType = u_lightPositions[i].w;
        vec3 lightColor = u_lightColors[i].rgb;
        float lightIntensity = u_lightColors[i].a;
        
        vec3 L;
        float attenuation = 1.0;
        
        if (lightType < 0.5) {
            // Directional light
            L = normalize(-u_lightPositions[i].xyz);
        } else {
            // Point light
            vec3 lightPos = u_lightPositions[i].xyz;
            vec3 lightDir = lightPos - v_worldPos;
            float distance = length(lightDir);
            L = normalize(lightDir);
            
            // Check if light is within range
            float range = u_lightParams[i].x;
            if (range > 0.0 && distance > range) {
                continue; // Skip this light if out of range
            }
            
            // Calculate attenuation
            float constant = u_lightParams[i].y;
            float linearTerm = u_lightParams[i].z;
            float quadratic = u_lightParams[i].w;
            attenuation = 1.0 / (constant + linearTerm * distance + quadratic * distance * distance);
        }
        
        finalColor += CalculatePBRLighting(N, V, L, baseColor, metallic, roughness, lightColor, lightIntensity) * attenuation;
    }

    // Exponential fog
    if (u_ambientFog.w > 0.5) {
        float distance = length(v_viewDir) > 0.0 ? length(v_worldPos - u_cameraPos.xyz) : 0.0;
        float fogFactor = 1.0 - clamp(exp(-u_fogParams.x * distance), 0.0, 1.0);
        vec3 fogColor = u_fogParams.yzw;
        finalColor = mix(finalColor, fogColor, fogFactor);
    }

    gl_FragColor = vec4(finalColor, 1.0);
}
