#version 410

in vec2 pos;
in vec2 uv;
in vec4 color;
out vec2 _uv;
out vec4 _color;
uniform mat4 projection;

void main(void) {
    _uv = uv;
    _color = color;
    gl_Position = projection * vec4(pos, 0, 1);
}
