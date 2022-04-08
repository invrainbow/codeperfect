#include <AppKit/AppKit.h>
#include <Availability.h>
#include <Cocoa/Cocoa.h>
#include "os.hpp"
#include "utils.hpp"

void init_platform_specific_crap() { }

// idk why but including world.hpp gives us a bunch of dumb conflicts
// i hate programming
void* get_native_window_handle();

NSAlert *make_nsalert(ccstr text, ccstr title) {
    auto alert = [[NSAlert alloc] init];

    if (!title) title = "Error";
    [alert setMessageText:[NSString stringWithUTF8String:title]];

    if (text) [alert setInformativeText:[NSString stringWithUTF8String:text]];

    [alert setAlertStyle:NSAlertStyleWarning];
    return alert;
}

int run_nsalert(NSAlert *alert) {
    [[alert window] setLevel:NSModalPanelWindowLevel];

    auto wnd = (__bridge NSWindow*)get_native_window_handle();
    if (!wnd) return [alert runModal];

    __block int ret = -1;
    [alert beginSheetModalForWindow:wnd
                  completionHandler:^(NSModalResponse response) {
                      ret = response;
                      [NSApp stopModal];
                  }];

    [NSApp runModalForWindow:wnd];
    [wnd makeKeyAndOrderFront:nil];
    return ret;
}

Ask_User_Result ask_user_yes_no(ccstr text, ccstr title, ccstr yeslabel, ccstr nolabel, bool cancel) {
    @autoreleasepool {
        auto alert = make_nsalert(text, title);
        int i = 0;

        {
            auto btn = [alert addButtonWithTitle:[NSString stringWithUTF8String:yeslabel]];
            [btn setKeyEquivalent:@"\r"];
            [btn highlight:YES];
            [btn setTag:i++];
        }

        {
            auto btn = [alert addButtonWithTitle:[NSString stringWithUTF8String:nolabel]];
            [btn setTag:i++];
            if (!cancel) {
                [btn setKeyEquivalent:@"\e"];
            }
        }

        if (cancel) {
            auto btn = [alert addButtonWithTitle:@"Cancel"];
            [btn setKeyEquivalent:@"\e"];
            [btn setTag:i++];
        }

        int sel = run_nsalert(alert);
        if (!sel) return ASKUSER_YES;
        if (sel == 1) return ASKUSER_NO;
        if (sel == 2 && cancel) return ASKUSER_CANCEL;
        return ASKUSER_ERROR;
    }
}

void tell_user(ccstr text, ccstr title) {
    @autoreleasepool {
        auto alert = make_nsalert(text, title);
        [alert addButtonWithTitle:@"OK"];

        run_nsalert(alert);
    }
}

bool let_user_select_file(Select_File_Opts* opts) {
    @autoreleasepool {
        NSSavePanel *panel = nil;
        // NSWindow *keyWindow = [[NSApplication sharedApplication] keyWindow];
        // [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyRegular];

        if (opts->save) {
            if (opts->folder) return false;

            auto savedlg = [NSSavePanel savePanel];
            [savedlg setExtensionHidden:NO];

            panel = (NSSavePanel*)savedlg;
        } else {
            NSOpenPanel *opendlg = [NSOpenPanel openPanel];
            [opendlg setAllowsMultipleSelection:NO];

            if (opts->folder) {
                [opendlg setCanChooseDirectories:YES];
                [opendlg setCanCreateDirectories:YES];
                [opendlg setCanChooseFiles:NO];
            }

            panel = (NSSavePanel*)opendlg;
        }

		[panel setLevel:CGShieldingWindowLevel()];

        {
            auto path = opts->starting_folder;
            if (path && path[0] != '\0') {
                auto str = [NSString stringWithUTF8String:path];
                auto url = [NSURL fileURLWithPath:str isDirectory:YES];
                [panel setDirectoryURL:url];
            }
        }

        int result = -1;

        auto wnd = (__bridge NSWindow*)get_native_window_handle();

        if (wnd == nil) {
            result = [panel runModal];
        } else {
            __block int ret = -1;
            [panel beginSheetModalForWindow:wnd
                          completionHandler:^(NSModalResponse response) {
                              ret = response;
                              [NSApp stopModal];
                          }];
            [NSApp runModalForWindow:wnd];
            result = ret;
        }

        [wnd makeKeyAndOrderFront:nil];

        if (result != NSModalResponseOK) return false;

        auto path = [[panel URL] path];
        auto pathstr = [path UTF8String];
        auto pathlen = [path lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

        if (pathlen + 1 > opts->bufsize) return false;

        strcpy_safe(opts->buf, opts->bufsize, pathstr);
        return true;
    }
}

void write_to_syslog(ccstr s) {
    NSLog(@"%s", s);
}

ccstr cfstring_to_ccstr(CFStringRef s) {
    if (s == NULL) return NULL;

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

List<ccstr> *get_font_cascade() {
    @autoreleasepool {
        auto sysfont = CTFontCreateUIFontForLanguage(kCTFontSystemFontType, 12.0, CFSTR("en-US"));

        CFStringRef langs[1] = { CFSTR("en") };
        auto cf_langs = CFArrayCreate(kCFAllocatorDefault, (const void**)langs, 1, &kCFTypeArrayCallBacks);

        auto fonts = CTFontCopyDefaultCascadeListForLanguages(sysfont, cf_langs);

        auto len = CFArrayGetCount(fonts);
        auto ret = alloc_list<ccstr>(len);

        for (auto i = 0; i < len; i++) {
            auto it = (CTFontDescriptorRef)CFArrayGetValueAtIndex(fonts, i);
            auto name = (CFStringRef)CTFontDescriptorCopyAttribute(it, kCTFontNameAttribute);
            ret->append(cfstring_to_ccstr(name));
        }

        return ret;
    }
}
