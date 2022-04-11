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

bool UI::init_fonts() {
    auto font = alloc_object(Font);
    if (!font->init("Menlo", CODE_FONT_SIZE)) return false;

    auto head = font;
    auto curr = font;

    auto add_font = [&](ccstr name) -> bool {
        auto next = alloc_object(Font);
        if (!next->init(name, CODE_FONT_SIZE)) return false;

        curr->next_fallback = next;
        curr = next;
        return true;
    };

    CFStringRef langs[1] = { CFSTR("en") };
    auto cf_langs = CFArrayCreate(NULL, (const void**)langs, 1, NULL);
    auto fallbacks = CTFontCopyDefaultCascadeListForLanguages((CTFontRef)font->ctfont, cf_langs);

    for (int i = 0, len = CFArrayGetCount(fallbacks); i < len; i++) {
        auto it = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fallbacks, i);
        auto name = (CFStringRef)CTFontDescriptorCopyAttribute(it, kCTFontNameAttribute);

        auto cname = cfstring_to_ccstr(name);
        if (cname[0] == '.') continue;

        if (!add_font(cname)) return false;
    }

    if (!add_font("Apple Symbols")) return false;

    font_list = head;
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

void* Font::draw_grapheme(List<uchar> codepoints_comprising_a_grapheme, s32 *psize) {
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

    auto device = CGColorSpaceCreateDeviceRGB();
    if (!device) return NULL;
    defer { CGColorSpaceRelease(device); };

    auto context = CGBitmapContextCreate(NULL, total_w, total_h, 8, total_w * 4, device, kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host);
    if (!context) return NULL;
    defer { CGContextRelease(context); };

    CGContextSetRGBFillColor(context, 0.0, 0.0, 0.0, 0.0);

    auto rect = CGRectMake(0, 0, total_w, total_h);
    CGContextFillRect(context, rect);

    if (opts.use_thin_strokes) {
        // TODO: CGContextSetFontSmoothingStyle(context, 16);
    }

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

    auto pixels = CGBitmapContextGetData(context);

    let rasterized_pixels = cg_context.data().to_vec();

    let buffer = if is_colored {
        BitmapBuffer::Rgba(byte_order::extract_rgba(&rasterized_pixels))
    } else {
        BitmapBuffer::Rgb(byte_order::extract_rgb(&rasterized_pixels))
    };

    RasterizedGlyph {
        character,
        left: rasterized_left,
        top: (bounds.size.height + bounds.origin.y).ceil() as i32,
        width: rasterized_width as i32, height: rasterized_height as i32,
        buffer,
    }
    */

    /*
    auto utf16_codepoints = alloc_list<unichar>();

    For (codepoints_comprising_a_grapheme) {
        // uchar -> utf-8
        char buf[4+1];
        auto nchars = uchar_to_cstr(it, buf);
        buf[nchars] = '\0';

        // utf-8 -> utf-16
        auto nsstr = [NSString stringWithUTF8String:buf];
        auto utf16chars = alloc_array(unichar, nsstr.length);
        [nsstr getCharacters:utf16chars range:NSMakeRange(0, nsstr.length)];

        // get glyph indexes
        auto glyph_indexes = alloc_array(CGGlyph, nsstr.length);
        if (!CTFontGetGlyphsForCharacters((CTFontRef)ctfont, utf16chars, glyph_indexes, nchars)) return NULL;

        for (int i = 0; i < nsstr.length; i++) {

        }

        // ???
    }
    */

    return NULL;

    /*

    auto bounds = CTFontGetBoundingRectsForGlyphs(
        (CTFontRef)ctfont,
        kCTFontDefaultOrientation,
        glyphs,
        NULL,
        glyphs.len


        self.as_concrete_TypeRef(),
                                            orientation,
                                            glyphs.as_ptr(),
                                            ptr::null_mut(),
                                            glyphs.len() as CFIndex)

    let bounds = self
        .ct_font
        .get_bounding_rects_for_glyphs(CTFontOrientation::default(), &[glyph_index as CGGlyph]);

    let rasterized_left = bounds.origin.x.floor() as i32;
    let rasterized_width =
        (bounds.origin.x - f64::from(rasterized_left) + bounds.size.width).ceil() as u32;
    let rasterized_descent = (-bounds.origin.y).ceil() as i32;
    let rasterized_ascent = (bounds.size.height + bounds.origin.y).ceil() as i32;
    let rasterized_height = (rasterized_descent + rasterized_ascent) as u32;

    if rasterized_width == 0 || rasterized_height == 0 {
        return RasterizedGlyph {
            character: ' ',
            width: 0,
            height: 0,
            top: 0,
            left: 0,
            buffer: BitmapBuffer::Rgb(Vec::new()),
        };
    }

    let mut cg_context = CGContext::create_bitmap_context(
        None,
        rasterized_width as usize, rasterized_height as usize,
        8, // bits per component
        rasterized_width as usize * 4,
        &CGColorSpace::create_device_rgb(),
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
    );

    let is_colored = self.is_colored();

    // Set background color for graphics context.
    let bg_a = if is_colored { 0.0 } else { 1.0 };
    cg_context.set_rgb_fill_color(0.0, 0.0, 0.0, bg_a);

    let context_rect = CGRect::new(
        &CGPoint::new(0.0, 0.0),
        &CGSize::new(f64::from(rasterized_width), f64::from(rasterized_height)),
    );

    cg_context.fill_rect(context_rect);

    if use_thin_strokes {
        cg_context.set_font_smoothing_style(16);
    }

    cg_context.set_allows_font_smoothing(true);
    cg_context.set_should_smooth_fonts(true);
    cg_context.set_allows_font_subpixel_quantization(true);
    cg_context.set_should_subpixel_quantize_fonts(true);
    cg_context.set_allows_font_subpixel_positioning(true);
    cg_context.set_should_subpixel_position_fonts(true);
    cg_context.set_allows_antialiasing(true);
    cg_context.set_should_antialias(true);

    // Set fill color to white for drawing the glyph.
    cg_context.set_rgb_fill_color(1.0, 1.0, 1.0, 1.0);
    let rasterization_origin =
        CGPoint { x: f64::from(-rasterized_left), y: f64::from(rasterized_descent) };

    self.ct_font.draw_glyphs(
        &[glyph_index as CGGlyph],
        &[rasterization_origin],
        cg_context.clone(),
    );

    let rasterized_pixels = cg_context.data().to_vec();

    let buffer = if is_colored {
        BitmapBuffer::Rgba(byte_order::extract_rgba(&rasterized_pixels))
    } else {
        BitmapBuffer::Rgb(byte_order::extract_rgb(&rasterized_pixels))
    };

    RasterizedGlyph {
        character,
        left: rasterized_left,
        top: (bounds.size.height + bounds.origin.y).ceil() as i32,
        width: rasterized_width as i32, height: rasterized_height as i32,
        buffer,
    }
    */
}

char* parse_hex(ccstr s) {
    int len = strlen(s);
    auto ret = alloc_array(char, len/2+1);
    int i = 0, j = 0;

    while (i<len) {
        while (isspace(s[i]) && i<len) i++;

        char lol[3];
        lol[0] = s[i+0];
        lol[1] = s[i+1];
        lol[2] = 0;
        ret[j++] = (char)strtol(lol, NULL, 16);

        i+=2;
    }

    ret[j] = 0;
    return ret;
}
