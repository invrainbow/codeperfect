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

    if (title != NULL) [alert setMessageText:[NSString stringWithUTF8String:title]];
    if (text != NULL) [alert setInformativeText:[NSString stringWithUTF8String:text]];

    [alert setAlertStyle:NSAlertStyleWarning];
    return alert;
}

int run_nsalert(NSAlert *alert) {
    [[alert window] setLevel:NSModalPanelWindowLevel];

    auto wnd = (__bridge NSWindow*)get_native_window_handle();
    if (wnd == NULL) return [alert runModal];

    __block int ret = -1;
    [alert beginSheetModalForWindow:wnd
                  completionHandler:^(NSModalResponse response) {
                      ret = response;
                      [NSApp stopModal];
                  }];

    [NSApp runModalForWindow:wnd];
    return ret;
}

Ask_User_Result ask_user_yes_no_cancel(ccstr text, ccstr title) {
    @autoreleasepool {
        auto alert = make_nsalert(text, title);
        [alert addButtonWithTitle:@"Yes"];
        [alert addButtonWithTitle:@"No"];
        [alert addButtonWithTitle:@"Cancel"];

        int sel = run_nsalert(alert);
        if (sel == NSAlertFirstButtonReturn) return ASKUSER_YES;
        if (sel == NSAlertSecondButtonReturn) return ASKUSER_NO;
        if (sel == NSAlertThirdButtonReturn) return ASKUSER_CANCEL;
        return ASKUSER_ERROR;
    }
}

Ask_User_Result ask_user_yes_no(ccstr text, ccstr title) {
    @autoreleasepool {
        auto alert = make_nsalert(text, title);
        [alert addButtonWithTitle:@"Yes"];
        [alert addButtonWithTitle:@"No"];

        int sel = run_nsalert(alert);
        if (sel == NSAlertFirstButtonReturn) return ASKUSER_YES;
        if (sel == NSAlertSecondButtonReturn) return ASKUSER_NO;
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
        NSWindow *keyWindow = [[NSApplication sharedApplication] keyWindow];
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
            if (path != NULL && path[0] != '\0') {
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

        [keyWindow makeKeyAndOrderFront:nil];

        if (result != NSModalResponseOK) return false;

        auto path = [[panel URL] path];
        auto pathstr = [path UTF8String];
        auto pathlen = [path lengthOfBytesUsingEncoding:NSUTF8StringEncoding];

        if (pathlen + 1 > opts->bufsize) return false;

        strcpy_safe(opts->buf, opts->bufsize, pathstr);
        return true;
    }
}
