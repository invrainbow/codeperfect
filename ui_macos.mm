#include <Cocoa/Cocoa.h>

#include <harfbuzz/hb.h>
#include <harfbuzz/hb-coretext.h>
#include <fontconfig/fontconfig.h>

#include "ui.hpp"
#include "common.hpp"
#include "list.hpp"
#include "defer.hpp"

// extern void CGContextSetFontSmoothingStyle(CGContextRef, int);
// extern int CGContextGetFontSmoothingStyle(CGContextRef);

static ccstr cfstring_to_ccstr(CFStringRef s) {
    if (!s) return NULL;

    CFIndex len = CFStringGetLength(s);
    CFIndex maxsize = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;

    Frame frame;

    auto ret = alloc_array(char, maxsize);
    if (!CFStringGetCString(s, ret, maxsize, kCFStringEncodingUTF8)) {
        frame.restore();
        return NULL;
    }
    return ret;
}

List<ccstr> *get_font_cascade(ccstr language) {
    @autoreleasepool {
        auto basefont = CTFontCreateWithName(CFSTR("Menlo"), 12, NULL);

        CFStringRef langs[1] = { CFStringCreateWithCString(NULL, language, kCFStringEncodingUTF8) };
        auto cf_langs = CFArrayCreate(NULL, (const void**)langs, 1, NULL);

        auto fonts = CTFontCopyDefaultCascadeListForLanguages(basefont, cf_langs);

        auto len = CFArrayGetCount(fonts);
        auto ret = alloc_list<ccstr>(len);

        for (auto i = 0; i < len; i++) {
            auto it = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fonts, i);
            auto name = (CFStringRef)CTFontDescriptorCopyAttribute(it, kCTFontNameAttribute);
            auto cname = cfstring_to_ccstr(name);
            if (cname[0] != '.') ret->append(cname);
        }

        ret->append("Apple Symbols");
        ret->append("DIN Condensed");
        return ret;
    }
}

Font* UI::acquire_font(ccstr name) {
    bool found = false;
    auto font = font_table.get(name, &found);
    if (found) return font;

    SCOPED_MEM(&ui_mem);
    Frame frame;

    font = alloc_object(Font);
    if (!font->init(name, CODE_FONT_SIZE)) {
        frame.restore();
        error("unable to acquire font: %s", name);
        return NULL;
    }

    font_table.set(name, font);
    return font;
}

bool UI::init_fonts() {
    // menlo has to succeed, or we literally don't have a font to use
    base_code_font = acquire_font("Menlo");
    if (!base_code_font) return false;

    // load some fallbacks
    acquire_font("Apple Symbols");

    CFStringRef langs[1] = { CFSTR("en") };
    auto cf_langs = CFArrayCreate(NULL, (const void**)langs, 1, NULL);
    auto fallbacks = CTFontCopyDefaultCascadeListForLanguages((CTFontRef)font->ctfont, cf_langs);

    for (int i = 0, len = CFArrayGetCount(fallbacks); i < len; i++) {
        auto it = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fallbacks, i);
        auto name = (CFStringRef)CTFontDescriptorCopyAttribute(it, kCTFontNameAttribute);

        auto cname = cfstring_to_ccstr(name);
        if (cname[0] == '.') continue;
        acquire_font(cname);
    }
    acquire_font("Apple Symbols");

    return true;
}

bool Font::init_font() {
    auto cf_font_name = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
    ctfont = (void*)CTFontCreateWithName(cf_font_name, (CGFloat)height, NULL);

    hbfont = hb_coretext_font_create((CTFontRef)ctfont);
    if (!hbfont) return NULL;

    return ctfont != NULL;
}

void Font::cleanup() {
    if (hbfont) {
        hb_font_destroy(hbfont);
        hbfont = NULL;
    }

    if (ctfont) {
        CFRelease(ctfont);
        ctfont = NULL;
    }
}

Rendered_Grapheme* Font::get_glyphs(List<uchar> codepoints_comprising_a_grapheme) {
    List<char> utf8_chars;
    char tmp[4];

    For (codepoints_comprising_a_grapheme) {
        int len = uchar_to_cstr(it, tmp);
        for (int i = 0; i < len; i++)
            utf8_chars.append(tmp[i]);
    }

    utf8_chars.append('\0');

    auto buf = hb_buffer_create();
    if (!buf) return NULL;
    defer { hb_buffer_destroy(buf); };

    hb_buffer_add_utf8(buf, utf8_chars.items, utf8_chars.len, 0, utf8_chars.len);
    hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    hb_buffer_set_language(buf, hb_language_from_string("en", -1));

    hb_shape(hbfont, buf, NULL, 0);

    unsigned int glyph_count;
    auto glyph_info = hb_buffer_get_glyph_infos(buf, &glyph_count);
    auto glyph_pos = hb_buffer_get_glyph_positions(buf, &glyph_count);

    struct Glyph_Info {
        uchar codepoint;

        i32 ras_x;
        u32 ras_w;
        i32 ras_desc;
        i32 ras_asc;
        u32 ras_h;
    };

    auto glyphs = alloc_list<Glyph_Info>();
    int total_w = 0;
    int total_h = 0;
    int cur_x = 0;
    int cur_y = 0;

    for (u32 i = 0; i < glyph_count; i++) {
        Glyph_Info gi;

        CGGlyph glyph = (CGGlyph)glyph_info[i].codepoint;

        auto rect = CTFontGetBoundingRectsForGlyphs((CTFontRef)ctfont, kCTFontDefaultOrientation, &glyph, NULL, 1);
        gi.ras_x = (i32)floor(rect.origin.x);
        gi.ras_w = (u32)ceil(rect.origin.x - gi.ras_x + rect.size.width);
        gi.ras_desc = (i32)ceil(-rect.origin.y);
        gi.ras_asc = (i32)ceil(rect.size.height + rect.origin.y);
        gi.ras_h = (u32)(gi.ras_desc + gi.ras_asc);

        if (!gi.ras_w && !gi.ras_h) { /* TODO? just don't append? */ }

        glyphs->append(&gi);

        auto right = cur_x + glyph_pos[i].x_offset + gi.ras_w;
        auto bottom = cur_y + glyph_pos[i].y_offset + gi.ras_h;

        if (right > total_w) total_w = right;
        if (bottom > total_h) total_h = bottom;

        cur_x += glyph_pos[i].x_advance;
        cur_y += glyph_pos[i].y_advance;
    }

    auto device = CGColorSpaceCreateDeviceGray();
    if (!device) return NULL;
    defer { CGColorSpaceRelease(device); };

    auto bitmap_buffer = alloc_array(char, total_w * total_h);

    auto context = CGBitmapContextCreate(bitmap_buffer, total_w, total_h, 8, total_w, device, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    if (!context) return NULL;
    defer { CGContextRelease(context); };

    CGContextSetRGBFillColor(context, 0.0, 0.0, 0.0, 0.0);

    auto rect = CGRectMake(0, 0, total_w, total_h);
    CGContextFillRect(context, rect);

    // TODO: CGContextSetFontSmoothingStyle(context, 16);
    CGContextSetAllowsFontSmoothing(context, true);
    CGContextSetShouldSmoothFonts(context, true);
    CGContextSetAllowsFontSubpixelQuantization(context, true);
    CGContextSetShouldSubpixelQuantizeFonts(context, true);
    CGContextSetAllowsFontSubpixelPositioning(context, true);
    CGContextSetShouldSubpixelPositionFonts(context, true);
    CGContextSetAllowsAntialiasing(context, true);
    CGContextSetShouldAntialias(context, true);

    CGContextSetRGBFillColor(context, 1.0, 1.0, 1.0, 1.0);

    cur_x = 0;
    cur_y = 0;

    for (int i = 0; i < glyph_count; i++) {
        auto &gi = glyphs->at(i);

        auto origin = CGPointMake(
            cur_x + glyph_pos[i].x_offset + gi.ras_x,
            cur_y + glyph_pos[i].y_offset + gi.ras_desc
        );

        unichar glyphs[2] = {0};
        bool ispair = CFStringGetSurrogatePairForLongCharacter(glyph_info[i].codepoint, glyphs);

        // when would we potentially draw two glyphs?
        for (int i = 0; i < (ispair ? 2 : 1); i++)
            if (glyphs[i])
                CTFontDrawGlyphs((CTFontRef)ctfont, &glyphs[i], &origin, 1, context);

        cur_x += glyph_pos[i].x_advance;
    }

    auto ret = alloc_object(Rendered_Grapheme);
    ret->data = bitmap_buffer;
    ret->data_len = total_w * total_h;
    ret->width = total_w;
    ret->height = total_h;
    return ret;
}
