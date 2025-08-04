#include <bgfx_shader.sh>

uniform vec4 u_pickColor;

void main() {
    gl_FragColor = u_pickColor;
}
