/*
 * Rut
 *
 * Copyright (C) 2012  Intel Corporation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see
 * <http://www.gnu.org/licenses/>.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <SDL.h>
#include <SDL_syswm.h>
#include <cogl/cogl.h>
#include <Cocoa/Cocoa.h>
#include <mach-o/dyld.h>
#include <unistd.h>
#include <glib.h>

#include "rig-osx.h"
#include "rig-data.h"
#include "rig-load-save.h"

@interface Controller : NSObject
{
  RigData *data;
}

@end

@implementation Controller

- (id) init:(RigData *) data_in
{
  self = [super init];

  if (self)
    self->data = data_in;

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

  dir = g_path_get_dirname (data->ui_filename);
  dir_string = [[NSString alloc] initWithUTF8String:dir];
  url = [NSURL fileURLWithPath:dir_string];
  [dialog setDirectoryURL:url];
  [dir_string release];
  g_free (dir);

  if ([dialog runModal] == NSOKButton)
    {
      NSArray *files = [dialog filenames];

      if ([files count] == 1)
	{
	  NSString *filename = [files objectAtIndex:0];

          data->next_ui_filename = g_strdup ([filename UTF8String]);
          rut_shell_quit (data->shell);
	}
    }
}

- (void) saveAction:(id) sender
{
  rig_save (data, data->ui_filename);
}

- (void) saveAsAction:(id) sender
{
}

- (BOOL) validateMenuItem:(NSMenuItem *) item
{
  return YES;
}

@end

typedef struct
{
  NSString *name;
  NSString *key;
  const char *selector;
} RigOSXMenuItem;

static const RigOSXMenuItem
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

  for (i = 0; i < 2; i++)
    {
      char *buf = g_malloc (size + 1);
      if (_NSGetExecutablePath (buf, &size) == -1)
	g_free (buf);
      else
	{
	  char *dn = g_path_get_dirname (buf);
	  char *pkg_root = g_build_filename (dn, "..", NULL);
	  g_free (dn);
	  g_free (buf);
	  return pkg_root;
	}
    }

  g_warn_if_reached ();

  return NULL;
}

static void
check_update_messages (void)
{
  char *pkg_root = get_package_root ();
  char *error_message;
  char *file_path;

  file_path = g_build_filename (pkg_root,
				"Resources",
				"update-fail-message.txt",
				NULL);
  if (g_file_get_contents (file_path,
			   &error_message,
			   NULL, /* length */
			   NULL /* error */))
    {
      NSString *error_message_str =
	[[NSString alloc] initWithUTF8String:error_message];
      NSString *full_message =
	[@"A previous automatic update failed with the following error:\n\n"
	  stringByAppendingString:error_message_str];

      NSRunAlertPanel (@"Automatic update error",
		       full_message,
		       @"Ok",
		       NULL, /* alternateButton */
		       NULL /* otherButton */);

      [full_message release];
      [error_message_str release];
      g_free (error_message);

      unlink (file_path);
    }
  g_free (file_path);

  file_path = g_build_filename (pkg_root,
				"Resources",
				"rig-updated",
				NULL);
  if (g_file_test (file_path, G_FILE_TEST_EXISTS))
    {
      NSRunAlertPanel (@"Automatic update",
		       @"Rig has been automatically updated to "
		       "the latest version",
		       @"Ok",
		       NULL, /* alternateButton */
		       NULL /* otherButton */);

      unlink (file_path);
    }
  g_free (file_path);

  g_free (pkg_root);
}

void
rig_osx_init (RigData *data)
{
  CoglOnscreen *onscreen = data->onscreen;
  SDL_Window *window = cogl_sdl_onscreen_get_window (onscreen);
  SDL_SysWMinfo info;
  NSMenuItem *file_item;
  NSMenu *file_menu;
  Controller *controller;
  NSAutoreleasePool *pool;
  int i;

  SDL_VERSION (&info.version);

  if (!SDL_GetWindowWMInfo (window, &info))
    return;

  pool = [[NSAutoreleasePool alloc] init];

  controller = [[Controller alloc] init:data];

  [info.info.cocoa.window setShowsResizeIndicator:YES];

  file_item = [[NSMenuItem allocWithZone:[NSMenu menuZone]]
                initWithTitle:@"File"
                action:NULL
                keyEquivalent:@""];
  file_menu = [[NSMenu allocWithZone:[NSMenu menuZone]]
                initWithTitle:@"File"];

  [file_item setSubmenu:file_menu];
  [[NSApp mainMenu] insertItem:file_item atIndex:1];

  for (i = 0; i < G_N_ELEMENTS (menu_items); i++)
    if (menu_items[i].name)
      {
        SEL selector = sel_registerName (menu_items[i].selector);
        NSMenuItem *item =
          [[NSMenuItem allocWithZone:[NSMenu menuZone]]
            initWithTitle:menu_items[i].name
            action:selector
            keyEquivalent:menu_items[i].key];

        [item setTarget:controller];

        [file_menu addItem:item];
        [item release];
      }
    else
      [file_menu addItem:[NSMenuItem separatorItem]];

  [file_menu release];
  [file_item release];

  check_update_messages ();
}
