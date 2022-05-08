#version 410

in vec2 pos;
in vec2 uv;
in vec4 color;
in int mode;
in int texture_id;
out vec2 _uv;
out vec4 _color;
flat out int _mode;
flat out int _texture_id;
uniform mat4 projection;

void main(void) {
    _uv = uv;
    _color = color;
    _mode = mode;
    _texture_id = texture_id;
    gl_Position = projection * vec4(pos, 0, 1);
}
