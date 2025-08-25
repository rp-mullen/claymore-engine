$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(sObjectId, 0);

uniform vec4 uTexelSize;        // (1/w, 1/h, 0, 0)
uniform vec4 uSelectedId;       // packed selected id in [0..1] rgb
uniform vec4 uParams;           // x: thickness in pixels (max 8), y: mode (0=selected-only, 1=global)

vec3 unpack(vec4 c){ return floor(c.rgb * 255.0 + 0.5); }

float isSelected(vec4 c)
{
    vec3 id = unpack(c);
    vec3 sid = unpack(uSelectedId);
    return (id.x == sid.x && id.y == sid.y && id.z == sid.z) ? 1.0 : 0.0;
}

void main()
{
    vec2 uv = v_texcoord0.xy;
    float mode = uParams.y; // 0 = selected-only, 1 = global
    vec2 t = uTexelSize.xy;
    float thick = clamp(uParams.x, 1.0, 8.0);

    float edge = 0.0;
    if (mode < 0.5) {
        // Selected-only: original behavior
        float csel = isSelected(texture2D(sObjectId, uv));
        if (csel < 0.5) { gl_FragColor = vec4_splat(0.0); return; }

        for (int i = 1; i <= 8; ++i) {
            float k = step(float(i) - 0.5, thick);
            if (k > 0.0) {
                vec2 o = vec2(float(i), float(i)) * t;
                // 4-connected
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2( o.x, 0.0))));
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2(-o.x, 0.0))));
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2(0.0,  o.y))));
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2(0.0, -o.y))));
                // diagonals for roundness
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2( o.x,  o.y))));
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2( o.x, -o.y))));
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2(-o.x,  o.y))));
                edge = max(edge, 1.0 - isSelected(texture2D(sObjectId, uv + vec2(-o.x, -o.y))));
            }
        }
        float mask = saturate(edge);
        gl_FragColor = vec4_splat(mask);
    } else {
        // Global: outline any boundary between differing non-zero object IDs
        vec3 cid = unpack(texture2D(sObjectId, uv));
        // Skip background pixels (id == 0)
        if (cid.x == 0.0 && cid.y == 0.0 && cid.z == 0.0) { gl_FragColor = vec4_splat(0.0); return; }

        for (int i = 1; i <= 8; ++i) {
            float k = step(float(i) - 0.5, thick);
            if (k > 0.0) {
                vec2 o = vec2(float(i), float(i)) * t;
                vec3 n0 = unpack(texture2D(sObjectId, uv + vec2( o.x, 0.0)));
                vec3 n1 = unpack(texture2D(sObjectId, uv + vec2(-o.x, 0.0)));
                vec3 n2 = unpack(texture2D(sObjectId, uv + vec2(0.0,  o.y)));
                vec3 n3 = unpack(texture2D(sObjectId, uv + vec2(0.0, -o.y)));
                vec3 n4 = unpack(texture2D(sObjectId, uv + vec2( o.x,  o.y)));
                vec3 n5 = unpack(texture2D(sObjectId, uv + vec2( o.x, -o.y)));
                vec3 n6 = unpack(texture2D(sObjectId, uv + vec2(-o.x,  o.y)));
                vec3 n7 = unpack(texture2D(sObjectId, uv + vec2(-o.x, -o.y)));

                float diff = 0.0;
                diff = max(diff, float(any(notEqual(n0, cid))));
                diff = max(diff, float(any(notEqual(n1, cid))));
                diff = max(diff, float(any(notEqual(n2, cid))));
                diff = max(diff, float(any(notEqual(n3, cid))));
                diff = max(diff, float(any(notEqual(n4, cid))));
                diff = max(diff, float(any(notEqual(n5, cid))));
                diff = max(diff, float(any(notEqual(n6, cid))));
                diff = max(diff, float(any(notEqual(n7, cid))));
                edge = max(edge, diff);
            }
        }
        gl_FragColor = vec4_splat(saturate(edge));
    }
}
