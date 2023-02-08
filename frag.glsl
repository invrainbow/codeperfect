#version 410

#define ROUND_TL (1 << 0)
#define ROUND_BL (1 << 1)
#define ROUND_TR (1 << 2)
#define ROUND_BR (1 << 3)

in vec2 _uv;
in vec4 _color;
flat in int _mode;
flat in int _texture_id;
flat in float _round_w;
flat in float _round_h;
flat in float _round_r;
flat in int _round_flags;
out vec4 outcolor;
uniform sampler2D tex;

vec4 cp_texture(vec2 uv) {
    return texture(tex, uv);
}

bool should_discard_for_rounded_rects() {
    float w = _round_w;
    float h = _round_h;
    float x = _uv.x * w;
    float y = _uv.y * h;
    float r = _round_r;

    if ((_round_flags & ROUND_TL) > 0)
        if (x < r && y < r)
            if (length(vec2(x,y) - vec2(r, r)) > r)
                return true;

    if ((_round_flags & ROUND_TR) > 0)
        if (x > w-r && y < r)
            if (length(vec2(x,y) - vec2(w-r, r)) > r)
                return true;

    if ((_round_flags & ROUND_BL) > 0)
        if (x < r && y > h-r)
            if (length(vec2(x,y) - vec2(r, h-r)) > r)
                return true;

    if ((_round_flags & ROUND_BR) > 0)
        if (x > w-r && y > h-r)
            if (length(vec2(x,y) - vec2(w-r, h-r)) > r)
                return true;

    return false;
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
    case 4: // DRAW_SOLID_ROUNDED
        if (should_discard_for_rounded_rects())
            discard;
        else
            outcolor = _color;
        break;
    }
}
