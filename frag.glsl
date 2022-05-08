#version 410

in vec2 _uv;
in vec4 _color;
flat in int _mode;
flat in int _texture_id;
out vec4 outcolor;
uniform sampler2D tex0;
uniform sampler2D tex1;
uniform sampler2D tex2;
uniform sampler2D tex3;
uniform sampler2D tex4;
uniform sampler2D tex5;

vec4 cp_texture(vec2 uv) {
    if (_texture_id == 0) return texture(tex0, uv);
    if (_texture_id == 1) return texture(tex1, uv);
    if (_texture_id == 2) return texture(tex2, uv);
    if (_texture_id == 3) return texture(tex3, uv);
    if (_texture_id == 4) return texture(tex4, uv);
    if (_texture_id == 5) return texture(tex5, uv);
    return vec4(0);
}

void main(void) {
    switch (_mode) {
    case 0: // DRAW_SOLID
        outcolor = _color;
        break;
    case 1: // DRAW_FONT_MASK
        outcolor = vec4(_color.rgb, cp_texture(_uv).r * _color.a);
        break;
    case 2: // DRAW_IMAGE
        outcolor = cp_texture(_uv);
        break;
    case 3: // DRAW_IMAGE_MASK
        // outcolor = vec4(_color.rgb, (0.5 + dot(vec3(0.33, 0.33, 0.33), cp_texture(_uv).rgb) * 0.5) * cp_texture(_uv).a);
        outcolor = vec4(_color.rgb, cp_texture(_uv).a);
        break;
    }
}
