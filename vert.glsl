#version 410

in vec2 pos;
in vec2 uv;
in vec4 color;
in int mode;
in int texture_id;
in float round_w;
in float round_h;
in float round_r;
in int round_flags;

out vec2 _uv;
out vec4 _color;
flat out int _mode;
flat out int _texture_id;

flat out float _round_w;
flat out float _round_h;
flat out float _round_r;
flat out int _round_flags;

uniform mat4 projection;

void main(void) {
    _uv = uv;
    _color = color;
    _mode = mode;
    _texture_id = texture_id;
    _round_w = round_w;
    _round_h = round_h;
    _round_r = round_r;
    _round_flags = round_flags;
    gl_Position = projection * vec4(pos, 0, 1);
}
