$input v_texcoord0, v_worldPos, v_normal, v_viewDir
#include <bgfx_shader.sh>

uniform vec4 uObjectId;     // xyz: packed id/255, w unused

void main()
{
    gl_FragColor = vec4(uObjectId.rgb, 1.0);
}
