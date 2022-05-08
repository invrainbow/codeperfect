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

Font* UI::acquire_font(ccstr name) {
    bool found = false;
    auto font = font_cache.get(name, &found);
    if (found) return font;

    SCOPED_MEM(&world.ui_mem);
    Frame frame;

    font = alloc_object(Font);
    if (!font->init(name, CODE_FONT_SIZE)) {
        frame.restore();
        error("unable to acquire font: %s", name);
        return NULL;
    }

    font_cache.set(cp_strdup(name), font);
    return font;
}

bool UI::init_fonts() {
    font_cache.init();
    glyph_cache.init();

    // menlo has to succeed, or we literally don't have a font to use
    base_font = acquire_font("Menlo");
    if (!base_font) return false;

    // load some fallbacks
    acquire_font("Apple Symbols");

    // load list of fonts
    {
        auto collection = CTFontCollectionCreateFromAvailableFonts(NULL);
        defer { CFRelease(collection); };

        auto descriptors = CTFontCollectionCreateMatchingFontDescriptors(collection);
        defer { CFRelease(descriptors); };

        int len = CFArrayGetCount(descriptors);

        all_font_names = alloc_list<ccstr>(len);
        all_font_urls = alloc_list<ccstr>(len);

        for (int i = 0; i < len; i++) {
            auto it = (CTFontDescriptorRef)CFArrayGetValueAtIndex(descriptors, i);

            auto name = CTFontDescriptorCopyAttribute(it, kCTFontNameAttribute);
            if (!name) continue;
            defer { CFRelease(name); };

            auto url = (CFURLRef)CTFontDescriptorCopyAttribute(it, kCTFontURLAttribute);
            if (!url) continue;
            defer { CFRelease(url); };

            auto cname = cfstring_to_ccstr((CFStringRef)name);
            if (streq(cname, "LastResort")) continue;
            if (cname[0] == '.') continue;

            auto filepath = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
            if (!filepath) continue;
            defer { CFRelease(filepath); };

            auto fileext = CFURLCopyPathExtension(url);
            if (!fileext) continue;
            defer { CFRelease(fileext); };

            auto cfileext = cfstring_to_ccstr(fileext);
            if (!streq(cfileext, "ttf") && !streq(cfileext, "ttc")) continue;

            auto curl = cfstring_to_ccstr(filepath);

            all_font_names->append(cname);
            all_font_urls->append(curl);
        }
    }


    return true;
}

bool Font::init_font() {
    auto screen = CGMainDisplayID();
    auto screensize = CGDisplayScreenSize(screen);
    auto bounds = CGDisplayBounds(screen);

    auto cf_font_name = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);

    float xscale = 1.0f, yscale = 1.0f;
    world.window->get_content_scale(&xscale, &yscale);

    ctfont = (void*)CTFontCreateWithName(cf_font_name, yscale * ceil(height * 72.0 / 96.0), NULL);
    if (!ctfont) return false;

    auto url = (CFURLRef)CTFontCopyAttribute((CTFontRef)ctfont, kCTFontURLAttribute);
    if (!url) return false;
    defer { CFRelease(url); };

    auto cffilepath = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (!cffilepath) return false;
    defer { CFRelease(cffilepath); };

    filepath = cfstring_to_ccstr(cffilepath);

    hbfont = hb_coretext_font_create((CTFontRef)ctfont);
    if (!hbfont) return false;

    auto _ctfont = (CTFontRef)ctfont;
    ascent = CTFontGetAscent(_ctfont);

    {
        auto glyph = (CGGlyph)('A');
        width = CTFontGetAdvancesForGlyphs(_ctfont, kCTFontDefaultOrientation, &glyph, NULL, 1);
        width /= xscale;
    }

    {
        auto descent = CTFontGetDescent(_ctfont);
        auto bounding_box = CTFontGetBoundingBox(_ctfont);
        offset_y = (CGRectGetHeight(bounding_box) - (ascent + descent)) / 2 / yscale;
    }

    return true;
}

bool Font::can_render_chars(List<uchar> *chars) {
    SCOPED_FRAME();

    // a "unichar" (apple's terminology, not mine) is a utf16 char, unlike our
    // uchar (utf32)
    auto unichars = alloc_list<unichar>();

    For (*chars) {
        unichar buf[2] = {0};
        auto is_pair = CFStringGetSurrogatePairForLongCharacter(it, buf);

        unichars->append(buf[0]);
        if (is_pair)
            unichars->append(buf[1]);
    }

    auto tmp = alloc_array(CGGlyph, unichars->len);
    return CTFontGetGlyphsForCharacters((CTFontRef)ctfont, unichars->items, tmp, unichars->len);
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

Font* UI::find_font_for_grapheme(List<uchar> *grapheme) {
    if (base_font->can_render_chars(grapheme))
        return base_font;

    auto pat = FcPatternCreate();
    if (!pat) return error("FcPatternCreate failed"), (Font*)NULL;
    defer { FcPatternDestroy(pat); };

    auto charset = FcCharSetCreate();
    if (!charset) return error("FcCharSetCreate failed"), (Font*)NULL;
    defer { FcCharSetDestroy(charset); };

    For (*grapheme)
        if (!FcCharSetAddChar(charset, it))
            return error("FcCharSetAddChar failed"), (Font*)NULL;

    if (!FcPatternAddCharSet(pat, FC_CHARSET, charset))
        return error("FcPatternAddCharSet failed"), (Font*)NULL;

    if (!FcPatternAddString(pat, FC_FONTFORMAT, (const FcChar8*)"TrueType"))
        return error("FcPatternAddString failed"), (Font*)NULL;

    FcConfigSubstitute(NULL, pat, FcMatchPattern);
    FcDefaultSubstitute(pat);

    FcResult result;
    auto match = FcFontMatch(NULL, pat, &result);
    if (!match) return error("FcFontMatch failed"), (Font*)NULL;
    defer { FcPatternDestroy(match); };

    FcChar8 *uncasted_name = NULL;
    if (FcPatternGetString(match, FC_POSTSCRIPT_NAME, 0, &uncasted_name) != FcResultMatch) {
        error("unable to get postscript name for font");
        return NULL;
    }

    auto name = cp_strdup((ccstr)uncasted_name);
    if (streq(name, "LastResort")) {
        bool found = false;
        For (*all_font_names) {
            auto font = acquire_font(it);
            if (font->can_render_chars(grapheme))
                return font;
        }
        return NULL;
    }

    auto font = acquire_font(name);
    return font->can_render_chars(grapheme) ? font : NULL;
}

Rendered_Grapheme* Font::get_glyphs(List<uchar> *grapheme) {
    auto utf8_chars = alloc_list<char>();
    char tmp[4];

    For (*grapheme) {
        int len = uchar_to_cstr(it, tmp);
        for (int i = 0; i < len; i++)
            utf8_chars->append(tmp[i]);
    }

    auto buf = hb_buffer_create();
    if (!buf) return NULL;
    defer { hb_buffer_destroy(buf); };

    hb_buffer_add_utf8(buf, utf8_chars->items, utf8_chars->len, 0, utf8_chars->len);
    // hb_buffer_set_direction(buf, HB_DIRECTION_LTR);
    // hb_buffer_set_script(buf, HB_SCRIPT_LATIN);
    // hb_buffer_set_language(buf, hb_language_from_string("en", -1));
    hb_buffer_guess_segment_properties(buf);
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
    int cur_x = 0;
    int cur_y = 0;

    int b_top = -1;
    int b_left = -1;
    int b_bottom = -1;
    int b_right = -1;

    int highest_asc = 0;

    for (u32 i = 0; i < glyph_count; i++) {
        Glyph_Info gi;

        CGGlyph glyph = (CGGlyph)glyph_info[i].codepoint;

        auto rect = CTFontGetBoundingRectsForGlyphs((CTFontRef)ctfont, kCTFontDefaultOrientation, &glyph, NULL, 1);
        gi.ras_x = (i32)floor(rect.origin.x);
        gi.ras_w = (u32)ceil(rect.origin.x - gi.ras_x + rect.size.width);
        gi.ras_desc = (i32)ceil(-rect.origin.y);
        gi.ras_asc = (i32)ceil(rect.size.height + rect.origin.y);
        gi.ras_h = (u32)(gi.ras_desc + gi.ras_asc); // wait, why don't we just set this to rect.size.height?

        if (gi.ras_asc > highest_asc) highest_asc = gi.ras_asc;
        if (!gi.ras_w && !gi.ras_h) { /* TODO? just don't append? */ }

        glyphs->append(&gi);

        auto &pos = glyph_pos[i];

        auto left = cur_x + pos.x_offset + gi.ras_x;
        auto top = cur_x + pos.y_offset;
        auto right = left + gi.ras_w;
        auto bottom = top + gi.ras_h;

        if (b_left == -1 || left < b_left) b_left = left;
        if (b_top == -1 || top < b_top) b_top = top;
        if (b_right == -1 || right < b_right) b_right = right;
        if (b_bottom == -1 || bottom < b_bottom) b_bottom = bottom;

        cur_x += pos.x_advance;
        cur_y += pos.y_advance;
    }

    auto device = CGColorSpaceCreateDeviceGray();
    if (!device) return NULL;
    defer { CGColorSpaceRelease(device); };

    auto total_w = max(1, b_right - b_left);
    auto total_h = max(1, b_bottom - b_top);

    auto bitmap_buffer = alloc_array(char, total_w * total_h);

    auto context = CGBitmapContextCreate(bitmap_buffer, total_w, total_h, 8, total_w, device, 0); // kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
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
            cur_x + glyph_pos[i].x_offset - gi.ras_x,
            (float)total_h - (float)(cur_y + glyph_pos[i].y_offset + gi.ras_asc)
        );

        unichar glyphs[2] = {0};
        bool ispair = CFStringGetSurrogatePairForLongCharacter(glyph_info[i].codepoint, glyphs);

        // when would we potentially draw two glyphs?
        for (int i = 0; i < (ispair ? 2 : 1); i++)
            if (glyphs[i])
                CTFontDrawGlyphs((CTFontRef)ctfont, &glyphs[i], &origin, 1, context);

        cur_x += glyph_pos[i].x_advance;
        cur_y += glyph_pos[i].y_advance;
    }

    auto ret = alloc_object(Rendered_Grapheme);
    ret->data = bitmap_buffer;
    ret->data_len = total_w * total_h;
    ret->box.x = b_left;
    ret->box.y = b_top + (ascent - highest_asc);
    ret->box.w = b_right - b_left;
    ret->box.h = b_bottom - b_top;
    return ret;
}
