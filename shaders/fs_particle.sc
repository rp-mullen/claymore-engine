$input v_color0, v_texcoord0

/*
 * Copyright 2011-2025 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx/blob/master/LICENSE
 */

#include <bgfx_shader.sh>

SAMPLER2D(s_texColor, 0);

void main()
{
    vec4 tex = texture2D(s_texColor, v_texcoord0.xy);
    // Color modulation by per-particle vertex color; use texture alpha for transparency.
    vec4 outCol = tex * v_color0;
    // Optional per-particle fade factor in v_texcoord0.z (0..1), multiply both rgb and alpha.
    outCol *= vec4(1.0, 1.0, 1.0, 1.0) * (1.0 - v_texcoord0.z + 0.0000);
    gl_FragColor = outCol;
}