#include "world.hpp"

#include <stdint.h>
#include <Carbon/Carbon.h>
#include <IOKit/hid/IOHIDLib.h>
#import <Cocoa/Cocoa.h>

#include "window.hpp"

static int scan_to_key_table[256];
static int key_to_scan_table[__CP_KEY_COUNT];

struct Winctx {
    id helper;
    id delegate;
    id keyup_monitor;
    CGEventSourceRef event_source;
} winctx;

@interface CPApplicationDelegate : NSObject <NSApplicationDelegate>
@end

@implementation CPApplicationDelegate

- (NSApplicationTerminateReply)applicationShouldTerminate:(NSApplication *)sender {
    if (world.window)
        world.window->should_close = true;
    return NSTerminateCancel;
}

- (void)applicationDidChangeScreenParameters:(NSNotification *) notification {
    if (world.window)
        [world.window->nsgl_object update];
}

void create_menu_bar() {
    NSMenu* bar = [[NSMenu alloc] init];
    [NSApp setMainMenu:bar];

    NSMenuItem* appMenuItem =
        [bar addItemWithTitle:@"CodePerfect" action:NULL keyEquivalent:@""];
    NSMenu* menu = [[NSMenu alloc] init];
    [appMenuItem setSubmenu:menu];

    [menu addItemWithTitle:[NSString stringWithFormat:@"Quit CodePerfect"]
                    action:@selector(terminate:)
             keyEquivalent:@"q"];

    // Prior to Snow Leopard, we need to use this oddly-named semi-private API
    // to get the application menu working properly.
    SEL setAppleMenuSelector = NSSelectorFromString(@"setAppleMenu:");
    [NSApp performSelector:setAppleMenuSelector withObject:menu];
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    create_menu_bar();
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    @autoreleasepool {
        NSEvent* event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                            location:NSMakePoint(0, 0)
                                       modifierFlags:0
                                           timestamp:0
                                        windowNumber:0
                                             context:nil
                                             subtype:0
                                               data1:0
                                               data2:0];
        [NSApp postEvent:event atStart:YES];
    }
    [NSApp stop:nil];
}

- (void)applicationDidHide:(NSNotification *)notification {
    // do we need this?
}

@end

@interface CPHelper : NSObject
@end

@implementation CPHelper
- (void)selectedKeyboardInputSourceChanged:(NSObject* )object {
    // updateUnicodeData();
}
- (void)doNothing:(id)object {
}
@end

bool window_init_everything() {
    ptr0(&winctx);

    memset(scan_to_key_table, -1, sizeof(scan_to_key_table));
    memset(key_to_scan_table, -1, sizeof(key_to_scan_table));

    scan_to_key_table[0x1D] = CP_KEY_0;
    scan_to_key_table[0x12] = CP_KEY_1;
    scan_to_key_table[0x13] = CP_KEY_2;
    scan_to_key_table[0x14] = CP_KEY_3;
    scan_to_key_table[0x15] = CP_KEY_4;
    scan_to_key_table[0x17] = CP_KEY_5;
    scan_to_key_table[0x16] = CP_KEY_6;
    scan_to_key_table[0x1A] = CP_KEY_7;
    scan_to_key_table[0x1C] = CP_KEY_8;
    scan_to_key_table[0x19] = CP_KEY_9;
    scan_to_key_table[0x00] = CP_KEY_A;
    scan_to_key_table[0x0B] = CP_KEY_B;
    scan_to_key_table[0x08] = CP_KEY_C;
    scan_to_key_table[0x02] = CP_KEY_D;
    scan_to_key_table[0x0E] = CP_KEY_E;
    scan_to_key_table[0x03] = CP_KEY_F;
    scan_to_key_table[0x05] = CP_KEY_G;
    scan_to_key_table[0x04] = CP_KEY_H;
    scan_to_key_table[0x22] = CP_KEY_I;
    scan_to_key_table[0x26] = CP_KEY_J;
    scan_to_key_table[0x28] = CP_KEY_K;
    scan_to_key_table[0x25] = CP_KEY_L;
    scan_to_key_table[0x2E] = CP_KEY_M;
    scan_to_key_table[0x2D] = CP_KEY_N;
    scan_to_key_table[0x1F] = CP_KEY_O;
    scan_to_key_table[0x23] = CP_KEY_P;
    scan_to_key_table[0x0C] = CP_KEY_Q;
    scan_to_key_table[0x0F] = CP_KEY_R;
    scan_to_key_table[0x01] = CP_KEY_S;
    scan_to_key_table[0x11] = CP_KEY_T;
    scan_to_key_table[0x20] = CP_KEY_U;
    scan_to_key_table[0x09] = CP_KEY_V;
    scan_to_key_table[0x0D] = CP_KEY_W;
    scan_to_key_table[0x07] = CP_KEY_X;
    scan_to_key_table[0x10] = CP_KEY_Y;
    scan_to_key_table[0x06] = CP_KEY_Z;
    scan_to_key_table[0x27] = CP_KEY_APOSTROPHE;
    scan_to_key_table[0x2A] = CP_KEY_BACKSLASH;
    scan_to_key_table[0x2B] = CP_KEY_COMMA;
    scan_to_key_table[0x18] = CP_KEY_EQUAL;
    scan_to_key_table[0x32] = CP_KEY_GRAVE_ACCENT;
    scan_to_key_table[0x21] = CP_KEY_LEFT_BRACKET;
    scan_to_key_table[0x1B] = CP_KEY_MINUS;
    scan_to_key_table[0x2F] = CP_KEY_PERIOD;
    scan_to_key_table[0x1E] = CP_KEY_RIGHT_BRACKET;
    scan_to_key_table[0x29] = CP_KEY_SEMICOLON;
    scan_to_key_table[0x2C] = CP_KEY_SLASH;
    scan_to_key_table[0x0A] = CP_KEY_WORLD_1;
    scan_to_key_table[0x33] = CP_KEY_BACKSPACE;
    scan_to_key_table[0x39] = CP_KEY_CAPS_LOCK;
    scan_to_key_table[0x75] = CP_KEY_DELETE;
    scan_to_key_table[0x7D] = CP_KEY_DOWN;
    scan_to_key_table[0x77] = CP_KEY_END;
    scan_to_key_table[0x24] = CP_KEY_ENTER;
    scan_to_key_table[0x35] = CP_KEY_ESCAPE;
    scan_to_key_table[0x7A] = CP_KEY_F1;
    scan_to_key_table[0x78] = CP_KEY_F2;
    scan_to_key_table[0x63] = CP_KEY_F3;
    scan_to_key_table[0x76] = CP_KEY_F4;
    scan_to_key_table[0x60] = CP_KEY_F5;
    scan_to_key_table[0x61] = CP_KEY_F6;
    scan_to_key_table[0x62] = CP_KEY_F7;
    scan_to_key_table[0x64] = CP_KEY_F8;
    scan_to_key_table[0x65] = CP_KEY_F9;
    scan_to_key_table[0x6D] = CP_KEY_F10;
    scan_to_key_table[0x67] = CP_KEY_F11;
    scan_to_key_table[0x6F] = CP_KEY_F12;
    scan_to_key_table[0x69] = CP_KEY_PRINT_SCREEN;
    scan_to_key_table[0x6B] = CP_KEY_F14;
    scan_to_key_table[0x71] = CP_KEY_F15;
    scan_to_key_table[0x6A] = CP_KEY_F16;
    scan_to_key_table[0x40] = CP_KEY_F17;
    scan_to_key_table[0x4F] = CP_KEY_F18;
    scan_to_key_table[0x50] = CP_KEY_F19;
    scan_to_key_table[0x5A] = CP_KEY_F20;
    scan_to_key_table[0x73] = CP_KEY_HOME;
    scan_to_key_table[0x72] = CP_KEY_INSERT;
    scan_to_key_table[0x7B] = CP_KEY_LEFT;
    scan_to_key_table[0x3A] = CP_KEY_LEFT_ALT;
    scan_to_key_table[0x3B] = CP_KEY_LEFT_CONTROL;
    scan_to_key_table[0x38] = CP_KEY_LEFT_SHIFT;
    scan_to_key_table[0x37] = CP_KEY_LEFT_SUPER;
    scan_to_key_table[0x6E] = CP_KEY_MENU;
    scan_to_key_table[0x47] = CP_KEY_NUM_LOCK;
    scan_to_key_table[0x79] = CP_KEY_PAGE_DOWN;
    scan_to_key_table[0x74] = CP_KEY_PAGE_UP;
    scan_to_key_table[0x7C] = CP_KEY_RIGHT;
    scan_to_key_table[0x3D] = CP_KEY_RIGHT_ALT;
    scan_to_key_table[0x3E] = CP_KEY_RIGHT_CONTROL;
    scan_to_key_table[0x3C] = CP_KEY_RIGHT_SHIFT;
    scan_to_key_table[0x36] = CP_KEY_RIGHT_SUPER;
    scan_to_key_table[0x31] = CP_KEY_SPACE;
    scan_to_key_table[0x30] = CP_KEY_TAB;
    scan_to_key_table[0x7E] = CP_KEY_UP;
    scan_to_key_table[0x52] = CP_KEY_KP_0;
    scan_to_key_table[0x53] = CP_KEY_KP_1;
    scan_to_key_table[0x54] = CP_KEY_KP_2;
    scan_to_key_table[0x55] = CP_KEY_KP_3;
    scan_to_key_table[0x56] = CP_KEY_KP_4;
    scan_to_key_table[0x57] = CP_KEY_KP_5;
    scan_to_key_table[0x58] = CP_KEY_KP_6;
    scan_to_key_table[0x59] = CP_KEY_KP_7;
    scan_to_key_table[0x5B] = CP_KEY_KP_8;
    scan_to_key_table[0x5C] = CP_KEY_KP_9;
    scan_to_key_table[0x45] = CP_KEY_KP_ADD;
    scan_to_key_table[0x41] = CP_KEY_KP_DECIMAL;
    scan_to_key_table[0x4B] = CP_KEY_KP_DIVIDE;
    scan_to_key_table[0x4C] = CP_KEY_KP_ENTER;
    scan_to_key_table[0x51] = CP_KEY_KP_EQUAL;
    scan_to_key_table[0x43] = CP_KEY_KP_MULTIPLY;
    scan_to_key_table[0x4E] = CP_KEY_KP_SUBTRACT;

    for (int i = 0; i < _countof(scan_to_key_table); i++) {
        auto it = scan_to_key_table[i];
        if (it != CP_KEY_UNKNOWN)
            key_to_scan_table[it] = i;
    }

    @autoreleasepool {
        winctx.helper = [[CPHelper alloc] init];

        [NSThread detachNewThreadSelector:@selector(doNothing:)
                                 toTarget:winctx.helper
                               withObject:nil];

        [NSApplication sharedApplication];

        winctx.delegate = [[CPApplicationDelegate alloc] init];
        if (winctx.delegate == nil) {
            error("failed to create application delegate");
            return false;
        }

        [NSApp setDelegate:winctx.delegate];

        // i don't know what the fuck this is for
        NSEvent* (^block)(NSEvent*) = ^ NSEvent* (NSEvent* event) {
            if ([event modifierFlags] & NSEventModifierFlagCommand)
                [[NSApp keyWindow] sendEvent:event];
            return event;
        };

        winctx.keyup_monitor =
            [NSEvent addLocalMonitorForEventsMatchingMask:NSEventMaskKeyUp
                                                  handler:block];

        // disable press and hold
        NSDictionary* defaults = @{@"ApplePressAndHoldEnabled":@NO};
        [[NSUserDefaults standardUserDefaults] registerDefaults:defaults];

        winctx.event_source = CGEventSourceCreate(kCGEventSourceStateHIDSystemState);
        if (!winctx.event_source) return false;
        CGEventSourceSetLocalEventsSuppressionInterval(winctx.event_source, 0.0);

        if (![[NSRunningApplication currentApplication] isFinishedLaunching])
            [NSApp run];

        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        return true;
    }
}

// translate cocoa keymods into cp keymod
int translate_keymod(NSUInteger flags) {
    int ret = 0;
    if (flags & NSEventModifierFlagShift) ret |= CP_MOD_SHIFT;
    if (flags & NSEventModifierFlagControl) ret |= CP_MOD_CTRL;
    if (flags & NSEventModifierFlagOption) ret |= CP_MOD_ALT;
    if (flags & NSEventModifierFlagCommand) ret |= CP_MOD_CMD;
    // if (flags & NSEventModifierFlagCapsLock)
    return ret;
}

// translate cocoa keycode to cp keycode
int translate_key(unsigned int key) {
    if (key >= _countof(scan_to_key_table))
        return CP_KEY_UNKNOWN;
    return scan_to_key_table[key];
}

void* Window::get_native_handle() {
    return ns_window;
}

void Window::update_cursor_image() {
    if (cursor)
        [(NSCursor*)cursor->object set];
    else
        [[NSCursor arrowCursor] set];
}

bool Window::is_cursor_in_content_area() {
    auto pos = [ns_window mouseLocationOutsideOfEventStream];
    return [ns_view mouse:pos inRect:[ns_view frame]];
}

// translate cp keycode to a cocoa modifier flag
NSUInteger translate_key_to_mod_flag(int key) {
    switch (key) {
        case CP_KEY_LEFT_SHIFT:
        case CP_KEY_RIGHT_SHIFT:
            return NSEventModifierFlagShift;
        case CP_KEY_LEFT_CONTROL:
        case CP_KEY_RIGHT_CONTROL:
            return NSEventModifierFlagControl;
        case CP_KEY_LEFT_ALT:
        case CP_KEY_RIGHT_ALT:
            return NSEventModifierFlagOption;
        case CP_KEY_LEFT_SUPER:
        case CP_KEY_RIGHT_SUPER:
            return NSEventModifierFlagCommand;
        case CP_KEY_CAPS_LOCK:
            return NSEventModifierFlagCapsLock;
    }
    return 0;
}

const NSRange kEmptyRange = { NSNotFound, 0 };

// ===============================
// ===== our window delegate =====
// ===============================

@interface CPWindowDelegate : NSObject {
    Window* window;
}

- (instancetype)initWithWindow:(Window *)initWindow;

@end

@implementation CPWindowDelegate

- (instancetype)initWithWindow:(Window *)initWindow {
    self = [super init];
    if (self != nil) window = initWindow;
    return self;
}

- (BOOL)windowShouldClose:(id)sender {
    window->should_close = true;
    return NO;
}

- (void)windowDidResize:(NSNotification *)notification {
    [window->nsgl_object update];

    window->maximized = [window->ns_window isZoomed];

    auto win_r = [window->ns_view frame];
    auto frame_r = [window->ns_view convertRectToBacking:win_r];

    if (frame_r.size.width != window->frame_w || frame_r.size.height != window->frame_h) {
        window->frame_w = frame_r.size.width;
        window->frame_h = frame_r.size.height;

        window->dispatch_event(WINEV_FRAME_SIZE, [&](auto ev) {
            ev->frame_size.w = window->frame_w;
            ev->frame_size.h = window->frame_h;
        });
    }

    if (win_r.size.width != window->window_w || win_r.size.height != window->window_h) {
        window->window_w = win_r.size.width;
        window->window_h = win_r.size.height;

        window->dispatch_event(WINEV_WINDOW_SIZE, [&](auto ev) {
            ev->window_size.w = window->window_w;
            ev->window_size.h = window->window_h;
        });
    }
}

- (void)windowDidBecomeKey:(NSNotification *)notification {
    if (window->is_cursor_in_content_area())
        window->update_cursor_image();
    window->dispatch_event(WINEV_FOCUS, [&](auto ev) {});
}

- (void)windowDidResignKey:(NSNotification *)notification {
    window->dispatch_event(WINEV_BLUR, [&](auto ev) {});
}

@end

//------------------------------------------------------------------------
// our content view class
//------------------------------------------------------------------------

@interface CPContentView : NSView <NSTextInputClient> {
    Window* window;
    NSTrackingArea* tracking_area;
    NSMutableAttributedString* marked_text;
}

- (instancetype)initWithWindow:(Window *)initWindow;

@end

@implementation CPContentView

- (instancetype)initWithWindow:(Window *)initWindow {
    self = [super init];
    if (self != nil) {
        window = initWindow;
        tracking_area = nil;
        marked_text = [[NSMutableAttributedString alloc] init];

        [self updateTrackingAreas];
        [self registerForDraggedTypes:@[NSPasteboardTypeURL]];
    }

    return self;
}

// basically, enable ctrl+tab and ctrl+shift+tab
- (BOOL)performKeyEquivalent:(NSEvent *)event {
    if ([[self window] firstResponder] != self) return NO;

    auto mods = [event modifierFlags];
    if (!(mods & NSEventModifierFlagControl)) return NO;

    auto code = [event keyCode];
    if (scan_to_key_table[code] != CP_KEY_TAB) return NO;

    if (![[NSApp mainMenu] performKeyEquivalent:event])
        [self keyDown:event];
    return YES;
}

- (BOOL)canBecomeKeyView { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)wantsUpdateLayer { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)event { return YES; }

- (void)updateLayer {
    [window->nsgl_object update];
}

- (void)cursorUpdate:(NSEvent *)event {
    window->update_cursor_image();
}

- (void)mouseDown:(NSEvent *)event {
    window->dispatch_mouse_event(CP_MOUSE_LEFT, true, translate_keymod([event modifierFlags]));
}

- (void)mouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)mouseUp:(NSEvent *)event {
    window->dispatch_mouse_event(CP_MOUSE_LEFT, false, translate_keymod([event modifierFlags]));
}

- (void)mouseMoved:(NSEvent *)event {
    const NSRect rect = [window->ns_view frame];
    // NOTE: The returned location uses base 0,1 not 0,0
    const NSPoint pos = [event locationInWindow];

    window->dispatch_event(WINEV_MOUSE_MOVE, [&](auto ev) {
        ev->mouse_move.x = pos.x;
        ev->mouse_move.y = rect.size.height - pos.y;
    });
}

- (void)rightMouseDown:(NSEvent *)event {
    window->dispatch_mouse_event(CP_MOUSE_RIGHT, true, translate_keymod([event modifierFlags]));
}

- (void)rightMouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)rightMouseUp:(NSEvent *)event {
    window->dispatch_mouse_event(CP_MOUSE_RIGHT, false, translate_keymod([event modifierFlags]));
}

- (void)otherMouseDown:(NSEvent *)event {
    int button = (int) [event buttonNumber];
    if (button != CP_MOUSE_MIDDLE) return; // TODO

    window->dispatch_mouse_event(CP_MOUSE_MIDDLE, true, translate_keymod([event modifierFlags]));
}

- (void)otherMouseDragged:(NSEvent *)event {
    [self mouseMoved:event];
}

- (void)otherMouseUp:(NSEvent *)event {
    int button = (int) [event buttonNumber];
    if (button != CP_MOUSE_MIDDLE) return; // TODO

    window->dispatch_mouse_event(CP_MOUSE_MIDDLE, false, translate_keymod([event modifierFlags]));
}

- (void)viewDidChangeBackingProperties {
    auto rect = [window->ns_view frame];
    auto framerect = [window->ns_view convertRectToBacking:rect];
    auto xscale = framerect.size.width / rect.size.width;
    auto yscale = framerect.size.height / rect.size.height;

    if (xscale != window->xscale || yscale != window->yscale) {
        window->xscale = xscale;
        window->yscale = yscale;

        window->dispatch_event(WINEV_WINDOW_SCALE, [&](auto ev) {
            ev->window_scale.xscale = xscale;
            ev->window_scale.yscale = yscale;
        });
    }

    if (framerect.size.width != window->frame_w || framerect.size.height != window->frame_h) {
        window->frame_w = framerect.size.width;
        window->frame_h = framerect.size.height;

        window->dispatch_event(WINEV_FRAME_SIZE, [&](auto ev) {
            ev->frame_size.w = framerect.size.width;
            ev->frame_size.h = framerect.size.height;
        });
    }
}

- (void)updateTrackingAreas {
    if (tracking_area != nil) {
        [self removeTrackingArea:tracking_area];
        tracking_area = nil;
    }

    const NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                          NSTrackingActiveInKeyWindow |
                                          NSTrackingEnabledDuringMouseDrag |
                                          NSTrackingCursorUpdate |
                                          NSTrackingInVisibleRect |
                                          NSTrackingAssumeInside;

    tracking_area = [[NSTrackingArea alloc] initWithRect:[self bounds]
                                                options:options
                                                  owner:self
                                               userInfo:nil];

    [self addTrackingArea:tracking_area];
    [super updateTrackingAreas];
}

- (void)keyDown:(NSEvent *)event {
    auto key = translate_key(scan);
    auto mods = translate_keymod([event modifierFlags]);

    window->key_states[key] = true;
    window->dispatch_event(WINEV_KEY, [&](auto ev) {
        ev->key.key = key;
        ev->key.press = true;
        ev->key.mods = mods;
    });

    auto prevent_default = [&]() -> bool {
        // custom key shortcuts that should not trigger insertText
        // will be harder when we allow user defined shortcuts, but cross that bridge when we get there
        if (mods == CP_MOD_ALT && key == CP_KEY_RIGHT_BRACKET) return true;
        if (mods == CP_MOD_ALT && key == CP_KEY_LEFT_BRACKET) return true;
        if (mods == (CP_MOD_PRIMARY | CP_MOD_ALT) && key == CP_KEY_R) return true;
        if (mods == (CP_MOD_ALT | CP_MOD_SHIFT) && key == CP_KEY_F) return true;
        if (mods == (CP_MOD_ALT | CP_MOD_SHIFT) && key == CP_KEY_O) return true;

        return false;
    };

    if (!prevent_default()) [self interpretKeyEvents:@[event]];
}

- (void)flagsChanged:(NSEvent *)event {
    const unsigned int modflags =
        [event modifierFlags] & NSEventModifierFlagDeviceIndependentFlagsMask;

    auto key = translate_key(scan);
    auto mods = translate_keymod(modflags);

    bool press;
    if (translate_key_to_mod_flag(key) & modflags)
        press = !window->key_states[key];
    else
        press = false;

    window->key_states[key] = press;
    window->dispatch_event(WINEV_KEY, [&](auto ev) {
        ev->key.key = key;
        ev->key.press = press;
        ev->key.mods = mods;
    });
}

- (void)keyUp:(NSEvent *)event {
    auto key = translate_key(scan);
    auto mods = translate_keymod([event modifierFlags]);

    window->key_states[key] = false;
    window->dispatch_event(WINEV_KEY, [&](auto ev) {
        ev->key.key = key;
        ev->key.press = false;
        ev->key.mods = mods;
    });
}

- (void)scrollWheel:(NSEvent *)event {
    double dx = [event scrollingDeltaX];
    double dy = [event scrollingDeltaY];

    if ([event hasPreciseScrollingDeltas]) {
        dx *= 0.1;
        dy *= 0.1;
    }

    if (fabs(dx) > 0.0 || fabs(dy) > 0.0) {
        window->dispatch_event(WINEV_SCROLL, [&](auto ev) {
            ev->scroll.dx = dx;
            ev->scroll.dy = dy;
        });
    }
}

/*
- (NSDragOperation)draggingEntered:(id <NSDraggingInfo>)sender {
    return NSDragOperationGeneric;
}

- (BOOL)performDragOperation:(id <NSDraggingInfo>)sender {
    // ...
}
*/

- (BOOL)hasMarkedText {
    return [marked_text length] > 0;
}

- (NSRange)markedRange {
    if ([marked_text length] > 0)
        return NSMakeRange(0, [marked_text length] - 1);
    return kEmptyRange;
}

- (NSRange)selectedRange {
    return kEmptyRange;
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
    marked_text = nil;
    if ([string isKindOfClass:[NSAttributedString class]])
        marked_text = [[NSMutableAttributedString alloc] initWithAttributedString:string];
    else
        marked_text = [[NSMutableAttributedString alloc] initWithString:string];
}

- (void)unmarkText {
    [[marked_text mutableString] setString:@""];
}

- (NSArray*)validAttributesForMarkedText {
    return [NSArray array];
}

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:(NSRangePointer)actualRange {
    return nil;
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
    return 0;
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualRange {
    const NSRect frame = [window->ns_view frame];
    return NSMakeRect(frame.origin.x, frame.origin.y, 0.0, 0.0);
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    NSString* characters;
    NSEvent* event = [NSApp currentEvent];
    const int mods = translate_keymod([event modifierFlags]);
    const int plain = !(mods & CP_MOD_CMD) && !(mods & CP_MOD_CTRL);
    if (!plain) return;

    if ([string isKindOfClass:[NSAttributedString class]])
        characters = [string string];
    else
        characters = (NSString*) string;

    NSRange range = NSMakeRange(0, [characters length]);
    while (range.length > 0) {
        uint32_t codepoint = 0;
        auto res = [characters getBytes:&codepoint
                              maxLength:sizeof(codepoint)
                             usedLength:NULL
                               encoding:NSUTF32StringEncoding
                                options:0
                                  range:range
                         remainingRange:&range];

        if (!res) continue;
        if (codepoint >= 0xf700 && codepoint <= 0xf7ff) continue;

        window->dispatch_event(WINEV_CHAR, [&](auto ev) {
            ev->character.ch = codepoint;
        });
    }
}

- (void)doCommandBySelector:(SEL)selector {
}

@end

//------------------------------------------------------------------------
// our window class
//------------------------------------------------------------------------

@interface CPWindow : NSWindow {}
@end

@implementation CPWindow
- (BOOL)canBecomeKeyWindow { return YES; }
- (BOOL)canBecomeMainWindow { return YES; }
@end

// transform y-coordinate between cgdisplay and ns screen spaces
float cocoa_transform_y(float y) {
    return CGDisplayBounds(CGMainDisplayID()).size.height - y - 1;
}

void Window::dispatch_mouse_event_cocoa(Mouse_Button button, bool press, void *event) {
    dispatch_mouse_event(button, press, translate_keymod([((__bridge NSEvent*)event) modifierFlags]));
}

void Window::make_context_current() {
    @autoreleasepool {
        [nsgl_object makeCurrentContext];
    }
}

void Window::swap_buffers() {
    @autoreleasepool {
        [nsgl_object flushBuffer];
    }
}

void Window::swap_interval(int interval) {
    @autoreleasepool {
        [nsgl_object setValues:&interval
                  forParameter:NSOpenGLContextParameterSwapInterval];
    }
}

void Window::create_nsgl_context() {
    auto attribs = alloc_list<NSOpenGLPixelFormatAttribute>();

    auto set_attrib = [&](auto a, auto v) {
        attribs->append(a);
        attribs->append(v);
    };

    attribs->append(NSOpenGLPFAAccelerated);
    attribs->append(NSOpenGLPFAClosestPolicy);

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
    set_attrib(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core);
#else
    set_attrib(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core);
#endif
    set_attrib(NSOpenGLPFAColorSize, 24);
    set_attrib(NSOpenGLPFAAlphaSize, 8);
    set_attrib(NSOpenGLPFADepthSize, 24);
    set_attrib(NSOpenGLPFAStencilSize, 8);
    attribs->append(NSOpenGLPFADoubleBuffer);
    set_attrib(NSOpenGLPFASampleBuffers, 0);
    attribs->append((NSOpenGLPixelFormatAttribute)0);

    nsgl_pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs->items];
    if (nsgl_pixel_format == nil)
        cp_panic("Unable to initialize window (couldn't find pixel format)");

    nsgl_object = [[NSOpenGLContext alloc] initWithFormat:nsgl_pixel_format shareContext:nil];
    if (nsgl_object == nil)
        cp_panic("Unable to initialize window (couldn't create context)");

    [ns_view setWantsBestResolutionOpenGLSurface:YES];
    [nsgl_object setView:ns_view];
}

bool Window::init_os_specific(int width, int height, ccstr title) {
    @autoreleasepool {
        if (!create_actual_window(width, height, title)) return false;
        create_nsgl_context();
        make_context_current();

        [ns_window orderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];
        [ns_window makeKeyAndOrderFront:nil];

        return true;
    }
}

void Window::cleanup() {
    @autoreleasepool {
        [ns_window orderOut:nil];
        nsgl_pixel_format = nil;
        nsgl_object = nil;
        ns_view = nil;

        [ns_window setDelegate:nil];
        ns_delegate = nil;

        [ns_window close];
        ns_window = nil;

        // allow cocoa to catch up before returning
        poll_window_events();
    }

    mem.cleanup();
}

void Window::set_title(ccstr title) {
    @autoreleasepool {
        NSString* string = @(title);
        [ns_window setTitle:string];
        [ns_window setMiniwindowTitle:string];
    }
}

void Window::get_size(int* width, int* height) {
    @autoreleasepool {
        const NSRect rect = [ns_view frame];
        if (width) *width = rect.size.width;
        if (height) *height = rect.size.height;
    }
}

void Window::get_framebuffer_size(int* width, int* height) {
    @autoreleasepool {
        auto rect = [ns_view frame];
        auto framerect = [ns_view convertRectToBacking:rect];

        if (width) *width = (int)framerect.size.width;
        if (height) *height = (int)framerect.size.height;
    }
}

void Window::get_content_scale(float* xscale, float* yscale) {
    @autoreleasepool {
        auto points = [ns_view frame];
        auto pixels = [ns_view convertRectToBacking:points];

        if (xscale) *xscale = (float)(pixels.size.width / points.size.width);
        if (yscale) *yscale = (float)(pixels.size.height / points.size.height);
    }
}

/*
void Window::restore() {
    @autoreleasepool {
        if ([ns_window isMiniaturized])
            [ns_window deminiaturize:nil];
        else if ([ns_window isZoomed])
            [ns_window zoom:nil];
    }
}

void Window::maximize() {
    @autoreleasepool {
        if (![ns_window isZoomed])
            [ns_window zoom:nil];
    }
}

void Window::show() {
    @autoreleasepool {
        [ns_window orderFront:nil];
    }
}

void Window::hide() {
    @autoreleasepool {
        [ns_window orderOut:nil];
    }
}

void Window::request_attention() {
    @autoreleasepool {
        [NSApp requestUserAttention:NSInformationalRequest];
    }
}

void Window::focus() {
    @autoreleasepool {
        // Make us the active application
        [NSApp activateIgnoringOtherApps:YES];
        [ns_window makeKeyAndOrderFront:nil];
    }
}
*/

bool Window::is_focused() {
    @autoreleasepool {
        return [ns_window isKeyWindow];
    }
}

void poll_window_events() {
    @autoreleasepool {
        while (true) {
            auto event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:[NSDate distantPast]
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
            if (!event) break;
            [NSApp sendEvent:event];
        }
    }
}

void Window::get_cursor_pos(double* xpos, double* ypos) {
    @autoreleasepool {
        auto rect = [ns_view frame];
        // NOTE: The returned location uses base 0,1 not 0,0
        auto pos = [ns_window mouseLocationOutsideOfEventStream];

        if (xpos) *xpos = pos.x;
        if (ypos) *ypos = rect.size.height - pos.y;
    }
}

bool Cursor::init(Cursor_Type _type) {
    ptr0(this);
    type = _type;

    @autoreleasepool {
        SEL sel = NULL;

        switch (type) {
        case CP_CUR_RESIZE_EW: sel = NSSelectorFromString(@"_windowResizeEastWestCursor"); break;
        case CP_CUR_RESIZE_NS: sel = NSSelectorFromString(@"_windowResizeNorthSouthCursor"); break;
        case CP_CUR_RESIZE_NWSE: sel = NSSelectorFromString(@"_windowResizeNorthWestSouthEastCursor"); break;
        case CP_CUR_RESIZE_NESW: sel = NSSelectorFromString(@"_windowResizeNorthEastSouthWestCursor"); break;
        }

        if (sel && [NSCursor respondsToSelector:sel]) {
            id ret = [NSCursor performSelector:sel];
            if ([ret isKindOfClass:[NSCursor class]])
                object = ret;
        }

        if (object == nil) {
            switch (type) {
            case CP_CUR_ARROW: object = [NSCursor arrowCursor]; break;
            case CP_CUR_IBEAM: object = [NSCursor IBeamCursor]; break;
            case CP_CUR_CROSSHAIR: object = [NSCursor crosshairCursor]; break;
            case CP_CUR_POINTING_HAND: object = [NSCursor pointingHandCursor]; break;
            case CP_CUR_RESIZE_EW: object = [NSCursor resizeLeftRightCursor]; break;
            case CP_CUR_RESIZE_NS: object = [NSCursor resizeUpDownCursor]; break;
            case CP_CUR_RESIZE_ALL: object = [NSCursor closedHandCursor]; break;
            case CP_CUR_NOT_ALLOWED: object = [NSCursor operationNotAllowedCursor]; break;
            }
        }

        return (object != nil);
    }
}

void Cursor::cleanup() {
    @autoreleasepool {
        object = nil;
    }
}

void Window::set_cursor(Cursor *_cursor) {
    cursor = _cursor;
    @autoreleasepool {
        if (is_cursor_in_content_area())
            update_cursor_image();
    }
}

void set_clipboard_string(ccstr string) {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard declareTypes:@[NSPasteboardTypeString] owner:nil];
        [pasteboard setString:@(string) forType:NSPasteboardTypeString];
    }
}

static ccstr static_clipboard_string = NULL;

// Calls malloc. Don't call in a tight loop.
ccstr get_clipboard_string() {
    @autoreleasepool {
        NSPasteboard* pasteboard = [NSPasteboard generalPasteboard];
        if (![[pasteboard types] containsObject:NSPasteboardTypeString])
            return NULL;

        NSString* object = [pasteboard stringForType:NSPasteboardTypeString];
        if (!object) return NULL;

        if (static_clipboard_string)
            free((void*)static_clipboard_string);
        static_clipboard_string = strdup([object UTF8String]);
        return static_clipboard_string;
    }
}

bool Window::create_actual_window(int width, int height, ccstr title) {
    ns_delegate = [[CPWindowDelegate alloc] initWithWindow:this];
    if (ns_delegate == nil)
        return error("failed to create window delegate"), false;

    NSUInteger mask = NSWindowStyleMaskMiniaturizable
        | NSWindowStyleMaskTitled
        | NSWindowStyleMaskClosable
        | NSWindowStyleMaskResizable;

    ns_window = [[CPWindow alloc]
        initWithContentRect:NSMakeRect(0, 0, width, height)
                  styleMask:mask
                    backing:NSBackingStoreBuffered
                      defer:NO];

    if (ns_window == nil)
        return error("failed to create window"), false;

    [(NSWindow*)ns_window center];
    [(NSWindow*)ns_window cascadeTopLeftFromPoint:NSPointFromCGPoint(CGPointZero)];

    const NSWindowCollectionBehavior behavior = NSWindowCollectionBehaviorFullScreenPrimary | NSWindowCollectionBehaviorManaged;
    [ns_window setCollectionBehavior:behavior];

    // [ns_window zoom:nil];

    // TODO: figure out frame autosave name, maybe that's it wasn't working before

    ns_view = [[CPContentView alloc] initWithWindow:this];

    [ns_window setContentView:ns_view];
    [ns_window makeFirstResponder:ns_view];
    [ns_window setTitle:@(title)];
    [ns_window setDelegate:ns_delegate];
    [ns_window setAcceptsMouseMovedEvents:YES];
    [ns_window setRestorable:NO];

#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101200
    if ([ns_window respondsToSelector:@selector(setTabbingMode:)])
        [ns_window setTabbingMode:NSWindowTabbingModeDisallowed];
#endif

    get_size(&window_w, &window_h);
    get_framebuffer_size(&frame_w, &frame_h);
    return true;
}

static NSOpenGLPixelFormat *bootstrap_pixel_format = nil;
static NSOpenGLContext *bootstrap_context = nil;

// pretty much copy pasted from Window::create_nsgl_context()
void _make_bootstrap_context() {
    auto attribs = alloc_list<NSOpenGLPixelFormatAttribute>();
    auto set_attrib = [&](auto a, auto v) {
        attribs->append(a);
        attribs->append(v);
    };

    attribs->append(NSOpenGLPFAAccelerated);
    attribs->append(NSOpenGLPFAClosestPolicy);
    attribs->append(NSOpenGLPFAAllowOfflineRenderers);
    attribs->append(kCGLPFASupportsAutomaticGraphicsSwitching);
#if MAC_OS_X_VERSION_MAX_ALLOWED >= 101000
    set_attrib(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core);
#else
    set_attrib(NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion3_2Core);
#endif
    set_attrib(NSOpenGLPFAColorSize, 24);
    set_attrib(NSOpenGLPFAAlphaSize, 8);
    set_attrib(NSOpenGLPFADepthSize, 24);
    set_attrib(NSOpenGLPFAStencilSize, 8);
    attribs->append(NSOpenGLPFADoubleBuffer);
    set_attrib(NSOpenGLPFASampleBuffers, 0);
    attribs->append((NSOpenGLPixelFormatAttribute)0);

    bootstrap_pixel_format = [[NSOpenGLPixelFormat alloc] initWithAttributes:attribs->items];
    if (bootstrap_pixel_format == nil)
        cp_panic("Unable to initialize window (couldn't find pixel format)");

    bootstrap_context = [[NSOpenGLContext alloc] initWithFormat:bootstrap_pixel_format shareContext:nil];
    if (bootstrap_context == nil)
        cp_panic("Unable to initialize window (couldn't create context)");

    [bootstrap_context makeCurrentContext];
}

void make_bootstrap_context() {
    @autoreleasepool {
        _make_bootstrap_context();
    }
}

void destroy_bootstrap_context() {
    @autoreleasepool {
        bootstrap_pixel_format = nil;
        bootstrap_context = nil;
    }
}
