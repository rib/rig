/*
 * Rig
 *
 * UI Engine & Editor
 *
 * Copyright (C) 2013  Intel Corporation
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

#include <rig-config.h>

#include <gtk/gtk.h>
#include <gdk/gdkx.h>

#include "rig-application.h"
#include "rig-load-save.h"
#include "rut-box-layout.h"

#ifdef USE_SDL
#include <cglib/cg-sdl.h>
#include <SDL.h>
#include <SDL_syswm.h>
#endif

G_DEFINE_TYPE(rig_application_t, rig_application, G_TYPE_APPLICATION);

#define RIG_APPLICATION_GET_PRIVATE(obj)                                       \
    (G_TYPE_INSTANCE_GET_PRIVATE(                                              \
         (obj), RIG_TYPE_APPLICATION, rig_application_private_t))

#define RIG_APPLICATION_MENU_PATH "/org/zeroone/Rig/rig/menus/appmenu"

struct _rig_application_private_t {
    rig_editor_t *editor;
    GDBusConnection *dbus_connection;
    GMenuModel *menu_model;
    unsigned int export_menu_id;
};

static void
new_activated(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
}

static bool
get_xwindow_from_onscreen(cg_onscreen_t *onscreen, Window *xwindow)
{
#ifdef USE_SDL

    SDL_SysWMinfo parent_window_info;

    SDL_VERSION(&parent_window_info.version);

#if (SDL_MAJOR_VERSION >= 2)
    {
        SDL_Window *sdl_window = cg_sdl_onscreen_get_window(onscreen);

        if (!SDL_GetWindowWMInfo(sdl_window, &parent_window_info))
            return false;
    }
#else /* SDL_MAJOR_VERSION */
    {
        if (!SDL_GetWMInfo(&parent_window_info))
            return false;
    }
#endif /* SDL_MAJOR_VERSION */

    if (parent_window_info.subsystem != SDL_SYSWM_X11)
        return false;

    *xwindow = parent_window_info.info.x11.window;

    return true;

#else /* USE_SDL */

    return false;

#endif /* USE_SDL */
}

static void
dialog_realized_cb(GtkWidget *dialog, cg_onscreen_t *onscreen)
{
    GdkWindow *dialog_window = gtk_widget_get_window(dialog);
    Window xwindow;

    if (!get_xwindow_from_onscreen(onscreen, &xwindow))
        return;

    XSetTransientForHint(GDK_WINDOW_XDISPLAY(dialog_window),
                         GDK_WINDOW_XID(dialog_window),
                         xwindow);
}

static void
open_activated(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    rig_application_t *app = user_data;
    rig_editor_t *editor = app->priv->editor;
    GtkWidget *dialog;

    dialog = gtk_file_chooser_dialoc_new("Open",
                                         NULL, /* parent */
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL,
                                         GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN,
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);

    /* Listen to the realize so we can set our GdkWindow to be transient
     * for Rig's GdkWindow */
    g_signal_connect_after(dialog, "realize", G_CALLBACK(dialog_realized_cb),
                           editor->frontend->onscreen->cg_onscreen);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT) {
        char *filename =
            gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        rig_editor_load_file(editor, filename);
    }

    gtk_widget_destroy(dialog);
}

static void
save_activated(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    rig_application_t *app = user_data;
    rig_editor_t *editor = app->priv->editor;

    rig_editor_save(editor);
}

static void
save_as_activated(GSimpleAction *action,
                  GVariant *parameter,
                  gpointer user_data)
{
    /* TODO */
}

static void
quit_activated(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
    rig_application_t *app = user_data;

    rut_shell_quit(app->priv->editor->shell);
}

static GActionEntry app_entries[] = {
    { "new", new_activated, NULL, NULL, NULL },
    { "open", open_activated, NULL, NULL, NULL },
    { "save", save_activated, NULL, NULL, NULL },
    { "save_as", save_as_activated, NULL, NULL, NULL },
    { "quit", quit_activated, NULL, NULL, NULL }
};

static void
rig_application_activate(GApplication *application)
{
}

static void
set_window_property(cg_onscreen_t *onscreen,
                    const char *name,
                    const char *value)
{
    GdkDisplay *display;
    Window xwindow;

    if (!get_xwindow_from_onscreen(onscreen, &xwindow))
        return;

    display = gdk_display_get_default();

    XChangeProperty(
        GDK_DISPLAY_XDISPLAY(display),
        xwindow,
        gdk_x11_get_xatom_by_name_for_display(display, name),
        gdk_x11_get_xatom_by_name_for_display(display, "UTF8_STRING"),
        8,
        PropModeReplace,
        (const unsigned char *)value,
        strlen(value));
}

static void
destroy_onscreen_cb(void *user_data)
{
    GApplication *application = user_data;

    g_application_release(application);
}

void
rig_application_add_onscreen(rig_application_t *app,
                             rut_shell_onscreen_t *onscreen)
{
    rig_application_private_t *priv = app->priv;
    static cg_user_data_key_t data_key;
    const char *value;

    /* The GApplication will be held while there are onscreens in a
     * similar way to how GTK works */
    g_application_hold(G_APPLICATION(app));
    cg_object_set_user_data(CG_OBJECT(onscreen->cg_onscreen),
                            &data_key, app, destroy_onscreen_cb);

    /* These properties are copied from what GtkApplicationWindow sets */
    value = g_application_get_application_id(G_APPLICATION(app));
    set_window_property(onscreen, "_GTK_APPLICATION_ID", value);

    if (priv->dbus_connection) {
        value = g_dbus_connection_get_unique_name(priv->dbus_connection);
        set_window_property(onscreen, "_GTK_UNIQUE_BUS_NAME", value);
    }

    value = g_application_get_dbus_object_path(G_APPLICATION(app));
    if (value)
        set_window_property(onscreen, "_GTK_APPLICATION_OBJECT_PATH", value);

    if (priv->export_menu_id)
        set_window_property(
            onscreen, "_GTK_APP_MENU_OBJECT_PATH", RIG_APPLICATION_MENU_PATH);
}

static void
rig_application_startup(GApplication *application)
{
    rig_application_t *app = (rig_application_t *)application;
    rig_application_private_t *priv = app->priv;

    G_APPLICATION_CLASS(rig_application_parent_class)->startup(application);

    g_action_map_add_action_entries(G_ACTION_MAP(application),
                                    app_entries,
                                    C_N_ELEMENTS(app_entries),
                                    application);

    priv->dbus_connection = g_application_get_dbus_connection(application);
    if (priv->dbus_connection) {
        char *ui_file;

        g_object_ref(priv->dbus_connection);

        ui_file = rut_find_data_file("rig.ui");

        if (ui_file) {
            GtkBuilder *builder;
            GMenuModel *menu_model;

            builder = gtk_builder_new();

            gtk_builder_add_from_file(builder, ui_file, NULL);

            c_free(ui_file);

            menu_model =
                G_MENU_MODEL(gtk_builder_get_object(builder, "app-menu"));

            if (menu_model) {
                GError *error = NULL;

                priv->export_menu_id = g_dbus_connection_export_menu_model(
                    priv->dbus_connection,
                    RIG_APPLICATION_MENU_PATH,
                    menu_model,
                    &error);

                if (priv->export_menu_id)
                    priv->menu_model = g_object_ref(menu_model);
                else {
                    c_warning("Failed to export GMenuModel: %s",
                              error->message);
                    g_clear_error(&error);
                }
            }

            g_object_unref(builder);
        }
    }
}

static void
rig_application_shutdown(GApplication *application)
{
    rig_application_t *app = (rig_application_t *)application;
    rig_application_private_t *priv = app->priv;

    if (priv->dbus_connection) {
        if (priv->export_menu_id) {
            g_dbus_connection_unexport_menu_model(priv->dbus_connection,
                                                  priv->export_menu_id);
            priv->export_menu_id = 0;
            g_object_unref(priv->menu_model);
            priv->menu_model = NULL;
        }

        g_object_unref(priv->dbus_connection);
        priv->dbus_connection = NULL;
    }

    G_APPLICATION_CLASS(rig_application_parent_class)->shutdown(application);
}

static void
rig_application_class_init(rig_application_class_t *klass)
{
    GApplicationClass *application_class = G_APPLICATION_CLASS(klass);

    application_class->startup = rig_application_startup;
    application_class->shutdown = rig_application_shutdown;
    application_class->activate = rig_application_activate;

    g_type_class_add_private(klass, sizeof(rig_application_private_t));
}

static void
rig_application_init(rig_application_t *self)
{
    self->priv = RIG_APPLICATION_GET_PRIVATE(self);
}

rig_application_t *
rig_application_new(rig_editor_t *editor)
{
    rig_application_t *self;

    g_set_application_name("Rig");

    self = g_object_new(
        RIG_TYPE_APPLICATION, "application-id", "org.zeroone.rig.rig", NULL);

    self->priv->editor = editor;

    return self;
}
