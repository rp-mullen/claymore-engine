$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(sMaskVis, 0);
SAMPLER2D(sMaskOcc, 1);
SAMPLER2D(sDepth,   2);

uniform vec4 uTexelSize; // (1/w, 1/h, 0, 0)
uniform vec4 uColor;     // outline color RGBA
uniform vec4 uParams;    // x=useOcc(0/1), y=alphaOcc, z=depthFadeScale, w=unused

float dilate1(sampler2D sm, vec2 uv)
{
    vec2 t = uTexelSize.xy;
    float c = texture2D(sm, uv).r;
    c = max(c, texture2D(sm, uv + vec2( t.x, 0.0)).r);
    c = max(c, texture2D(sm, uv + vec2(-t.x, 0.0)).r);
    c = max(c, texture2D(sm, uv + vec2(0.0,  t.y)).r);
    c = max(c, texture2D(sm, uv + vec2(0.0, -t.y)).r);
    return c;
}

// 2px dilation with diagonals to increase thickness and roundness
float dilate2(sampler2D sm, vec2 uv)
{
    vec2 t = uTexelSize.xy;
    float c = 0.0;
    // radius 0/1
    c = max(c, texture2D(sm, uv).r);
    c = max(c, texture2D(sm, uv + vec2( t.x, 0.0)).r);
    c = max(c, texture2D(sm, uv + vec2(-t.x, 0.0)).r);
    c = max(c, texture2D(sm, uv + vec2(0.0,  t.y)).r);
    c = max(c, texture2D(sm, uv + vec2(0.0, -t.y)).r);
    c = max(c, texture2D(sm, uv + vec2( t.x,  t.y)).r);
    c = max(c, texture2D(sm, uv + vec2( t.x, -t.y)).r);
    c = max(c, texture2D(sm, uv + vec2(-t.x,  t.y)).r);
    c = max(c, texture2D(sm, uv + vec2(-t.x, -t.y)).r);
    // radius 2
    vec2 t2 = t * 2.0;
    c = max(c, texture2D(sm, uv + vec2( t2.x, 0.0)).r);
    c = max(c, texture2D(sm, uv + vec2(-t2.x, 0.0)).r);
    c = max(c, texture2D(sm, uv + vec2(0.0,  t2.y)).r);
    c = max(c, texture2D(sm, uv + vec2(0.0, -t2.y)).r);
    return c;
}

void main()
{
    vec2 uv = v_texcoord0.xy;

    float mVis0 = texture2D(sMaskVis, uv).r;
    float mVis1 = dilate2(sMaskVis, uv);
    float outlineVis = saturate(mVis1 - mVis0);

    float outlineOcc = 0.0;
    if (uParams.x > 0.5) {
        float mOcc0 = texture2D(sMaskOcc, uv).r;
        float mOcc1 = dilate2(sMaskOcc, uv);
        outlineOcc = saturate(mOcc1 - mOcc0) * uParams.y;
    }

    float outline = outlineVis + outlineOcc;

    // Lightweight antialiasing: box filter around edge
    vec2 t = uTexelSize.xy;
    float s = 0.0;
    s += outline;
    s += saturate(dilate1(sMaskVis, uv + vec2( 0.5*t.x,  0.0)) - mVis0);
    s += saturate(dilate1(sMaskVis, uv + vec2(-0.5*t.x,  0.0)) - mVis0);
    s += saturate(dilate1(sMaskVis, uv + vec2( 0.0,  0.5*t.y)) - mVis0);
    s += saturate(dilate1(sMaskVis, uv + vec2( 0.0, -0.5*t.y)) - mVis0);
    outline = s / 5.0;

    vec4 col = uColor * outline;
    gl_FragColor = col;
}


