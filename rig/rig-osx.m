/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy,
 * modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <config.h>

#include <SDL.h>
#include <SDL_syswm.h>
#include <cogl/cogl.h>
#include <cogl/cogl-sdl.h>
#include <Cocoa/Cocoa.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#include <glib.h>

#include "rig-osx.h"
#include "rig-engine.h"
#include "rig-load-save.h"

@interface Controller : NSObject
{
  rig_engine_t *engine;
}

@end

@implementation Controller

- (id) init:(rig_engine_t *) engine_in
{
  self = [super init];

  if (self)
    self->engine = engine_in;

  return self;
}

- (void) newFileAction:(id) sender
{
}

- (void) openAction:(id) sender
{
    NSOpenPanel *dialog = [NSOpenPanel openPanel];
    char *dir;
    NSString *dir_string;
    NSURL *url;

    [dialog setCanChooseFiles:YES];
    [dialog setCanChooseDirectories:NO];
    [dialog setAllowsMultipleSelection:NO];

    dir = c_path_get_dirname(engine->ui_filename);
    dir_string = [[NSString alloc] initWithUTF8String:dir];
    url = [NSURL fileURLWithPath:dir_string];
    [dialog setDirectoryURL:url];
    [dir_string release];
    c_free(dir);

    if ([dialog runModal] == NSOKButton) {
        NSArray *files = [dialog filenames];

        if ([files count] == 1) {
            NSString *filename = [files objectAtIndex:0];

            rig_frontend_load_file(engine->frontend, [filename UTF8String]);
        }
    }
}

- (void) saveAction:(id) sender
{
  rig_save(engine, engine->ui_filename);
}

- (void) saveAsAction:(id) sender
{
}

- (BOOL) validateMenuItem:(NSMenuItem *) item
{
  return YES;
}

@end

struct _rig_osx_engine
{
    NSMenuItem *file_item;
    NSMenu *file_menu;
    Controller *controller;
    NSAutoreleasePool *pool;
};

typedef struct
{
    NSString *name;
    NSString *key;
    const char *selector;
} rig_osx_menu_item_t;

static const rig_osx_menu_item_t
menu_items[] =
  {
    { @"New File...", @"n", "newFileAction:" },
    { @"Open...", @"o", "openAction:" },
    { NULL, NULL },
    { @"Save", @"s", "saveAction:" },
    { @"Save As...", @"S", "saveAsAction:" }
  };

static char *
get_package_root (void)
{
    uint32_t size = 4;
    int i;

    for (i = 0; i < 2; i++) {
        char *buf = c_malloc(size + 1);
        if (_NSGetExecutablePath(buf, &size) == -1)
            c_free(buf);
        else
        {
            char *dn = c_path_get_dirname (buf);
            char *pkg_root = c_build_filename(dn, "..", NULL);
            c_free(dn);
            c_free(buf);
            return pkg_root;
        }
    }

    c_warn_if_reached();

    return NULL;
}

static void
check_update_messages (void)
{
    char *pkg_root = get_package_root();
    char *error_message;
    char *file_path;

    file_path = c_build_filename(pkg_root,
                                 "Resources",
                                 "update-fail-message.txt",
                                 NULL);
    if (c_file_get_contents(file_path,
                            &error_message,
                            NULL, /* length */
                            NULL /* error */))
    {
        NSString *error_message_str =
            [[NSString alloc] initWithUTF8String:error_message];
        NSString *full_message =
            [@"A previous automatic update failed with the following error:\n\n"
             stringByAppendingString:error_message_str];

        NSRunAlertPanel(@"Automatic update error",
                        full_message,
                        @"Ok",
                        NULL, /* alternateButton */
                        NULL /* otherButton */);

        [full_message release];
        [error_message_str release];
        c_free(error_message);

        unlink(file_path);
    }
    c_free(file_path);

    file_path = c_build_filename(pkg_root,
                                 "Resources",
                                 "rig-updated",
                                 NULL);
    if (c_file_test(file_path, C_FILE_TEST_EXISTS)) {
        NSRunAlertPanel(@"Automatic update",
                        @"Rig has been automatically updated to "
                        "the latest version",
                        @"Ok",
                        NULL, /* alternateButton */
                        NULL /* otherButton */);

        unlink(file_path);
    }
    c_free(file_path);

    c_free(pkg_root);
}

void
rig_osx_init (rig_engine_t *engine)
{
    rig_osx_engine_t *osx_engine;
    cg_onscreen_t *onscreen = engine->frontend->onscreen;
    SDL_Window *window = cogl_sdl_onscreen_get_window (onscreen);
    SDL_SysWMinfo info;
    int i;

    osx_engine = c_new0(rig_osx_engine_t, 1);
    engine->osx_engine = osx_engine;

    SDL_VERSION (&info.version);

    if (!SDL_GetWindowWMInfo (window, &info))
        return;

    osx_engine->pool = [[NSAutoreleasePool alloc] init];

    osx_engine->controller = [[Controller alloc] init:engine];

    [info.info.cocoa.window setShowsResizeIndicator:YES];

    osx_engine->file_item = [[NSMenuItem allocWithZone:[NSMenu menuZone]]
                                         initWithTitle:@"File"
                                                action:NULL
                                         keyEquivalent:@""];
    osx_engine->file_menu = [[NSMenu allocWithZone:[NSMenu menuZone]]
                                     initWithTitle:@"File"];

    [osx_engine->file_item setSubmenu:osx_engine->file_menu];
    [[NSApp mainMenu] insertItem:osx_engine->file_item atIndex:1];

    for (i = 0; i < G_N_ELEMENTS (menu_items); i++)
        if (menu_items[i].name) {
            SEL selector = sel_registerName (menu_items[i].selector);
            NSMenuItem *item =
                [[NSMenuItem allocWithZone:[NSMenu menuZone]]
                             initWithTitle:menu_items[i].name
                                    action:selector
                             keyEquivalent:menu_items[i].key];

            [item setTarget:osx_engine->controller];

            [osx_engine->file_menu addItem:item];
            [item release];
        }
        else
            [osx_engine->file_menu addItem:[NSMenuItem separatorItem]];

    check_update_messages ();
}

void
rig_osx_deinit (rig_engine_t *engine)
{
    rig_osx_engine_t *osx_engine = engine->osx_engine;

    if (osx_engine->file_menu)
        [osx_engine->file_menu release];
    if (osx_engine->file_item) {
        [[NSApp mainMenu] removeItem:osx_engine->file_item];
        [osx_engine->file_item release];
    }

    [osx_engine->controller release];
    [osx_engine->pool release];

    c_free(osx_engine);
}
