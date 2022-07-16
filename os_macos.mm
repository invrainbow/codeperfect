#include <AppKit/AppKit.h>
#include <Availability.h>
#include <Cocoa/Cocoa.h>
#include "os.hpp"
#include "utils.hpp"
#include "defer.hpp"

void init_platform_specific_crap() { }

// idk why but including world.hpp gives us a bunch of dumb conflicts
// i hate programming
void* get_native_window_handle();

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

        cp_strcpy(opts->buf, opts->bufsize, pathstr);
        return true;
    }
}

void write_to_syslog(ccstr s) {
    NSLog(@"%s", s);
}

bool list_all_fonts(List<ccstr> *out) {
    auto collection = CTFontCollectionCreateFromAvailableFonts(NULL);
    defer { CFRelease(collection); };

    auto descriptors = CTFontCollectionCreateMatchingFontDescriptors(collection);
    defer { CFRelease(descriptors); };

    int len = CFArrayGetCount(descriptors);
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

        out->append(cname);
    }

    return true;
}

bool load_font_data_by_name(ccstr name, u8** data, u32 *len) {
    auto cf_font_name = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);

    auto ctfont = (void*)CTFontCreateWithName(cf_font_name, 12, NULL);
    if (!ctfont) return false;

    auto url = (CFURLRef)CTFontCopyAttribute((CTFontRef)ctfont, kCTFontURLAttribute);
    if (!url) return false;
    defer { CFRelease(url); };

    auto cffilepath = CFURLCopyFileSystemPath(url, kCFURLPOSIXPathStyle);
    if (!cffilepath) return false;
    defer { CFRelease(cffilepath); };

    auto fm = map_file_into_memory(cfstring_to_ccstr(cffilepath));
    if (!fm) return false;
    defer { fm->cleanup(); };

    auto ret = (u8*)malloc(fm->len);
    memcpy(ret, fm->data, fm->len);

    *len = fm->len;
    *data = ret;
    return true;
}
