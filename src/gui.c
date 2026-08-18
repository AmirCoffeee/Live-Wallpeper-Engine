#define _POSIX_C_SOURCE 200809L
#include "gui.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/stat.h>

#define AUTOSTART_FILE "live-wallpaper-autostart.desktop"
#define AUTOSTART_SH   "live-wallpaper-start.sh"

/* ------------------------------------------------------------------ */
/*  Status                                                              */
/* ------------------------------------------------------------------ */
static void update_status(AppGui *gui)
{
    if (wallpaper_engine_state(gui->engine) == WP_STATE_PLAYING &&
        gui->engine->current_file) {
        gchar *base = g_path_get_basename(gui->engine->current_file);
        gchar *m = g_strdup_printf(
            "<span foreground='#27ae60' weight='bold'>● Live: %s</span>", base);
        gtk_label_set_markup(GTK_LABEL(gui->status_label), m);
        g_free(m); g_free(base);

        /* Update tray tooltip too */
        if (gui->tray_icon) {
            gchar *tip = g_strdup_printf("Live Wallpaper — %s", base);
            gtk_status_icon_set_tooltip_text(gui->tray_icon, tip);
            g_free(tip);
        }
    } else {
        gtk_label_set_markup(GTK_LABEL(gui->status_label),
            "<span foreground='#7f8c8d'>● No wallpaper active</span>");
        if (gui->tray_icon)
            gtk_status_icon_set_tooltip_text(gui->tray_icon, "Live Wallpaper Engine");
    }
}

/* ------------------------------------------------------------------ */
/*  Autostart                                                           */
/* ------------------------------------------------------------------ */
static void write_autostart(gboolean enable)
{
    gchar *cfg_dir   = g_build_filename(g_get_home_dir(), ".config", "autostart", NULL);
    gchar *desk_file = g_build_filename(cfg_dir, AUTOSTART_FILE, NULL);
    gchar *sh_file   = g_build_filename(cfg_dir, AUTOSTART_SH,   NULL);

    if (enable) {
        g_mkdir_with_parents(cfg_dir, 0755);

        gchar *exe = g_find_program_in_path("live-wallpaper");
        if (!exe) exe = g_strdup("/usr/local/bin/live-wallpaper");

        gchar *sh_content = g_strdup_printf(
            "#!/bin/bash\n"
            "# Live Wallpaper Engine autostart\n"
            "sleep 3\n"
            "exec \"%s\"\n", exe);

        GError *err = NULL;
        g_file_set_contents(sh_file, sh_content, -1, &err);
        if (err) { g_warning("sh write: %s", err->message); g_error_free(err); }
        else     { chmod(sh_file, 0755); }
        g_free(sh_content);

        gchar *dc = g_strdup_printf(
            "[Desktop Entry]\nType=Application\nName=Live Wallpaper Engine\n"
            "Exec=bash \"%s\"\nHidden=false\nX-GNOME-Autostart-enabled=true\n"
            "Comment=Start Live Wallpaper Engine on login\n", sh_file);
        err = NULL;
        g_file_set_contents(desk_file, dc, -1, &err);
        if (err) { g_warning("desktop write: %s", err->message); g_error_free(err); }
        g_free(dc); g_free(exe);
    } else {
        remove(desk_file);
        remove(sh_file);
    }
    g_free(cfg_dir); g_free(desk_file); g_free(sh_file);
}

/* ------------------------------------------------------------------ */
/*  Switch callbacks                                                    */
/* ------------------------------------------------------------------ */
static void on_autostart(GObject *sw, GParamSpec *ps, gpointer d)
{
    (void)ps; AppGui *gui = d;
    gui->config->autostart = gtk_switch_get_active(GTK_SWITCH(sw));
    write_autostart(gui->config->autostart);
    config_save(gui->config);
}
static void on_mute(GObject *sw, GParamSpec *ps, gpointer d)
{
    (void)ps; AppGui *gui = d;
    gui->config->mute = gtk_switch_get_active(GTK_SWITCH(sw));
    config_save(gui->config);
}

/* ------------------------------------------------------------------ */
/*  Tray icon menu                                                      */
/* ------------------------------------------------------------------ */
static void tray_on_show_gallery(GtkMenuItem *item, gpointer d)
{
    (void)item; AppGui *gui = d;
    gallery_window_show(gui->gallery);
}
static void tray_on_show_settings(GtkMenuItem *item, gpointer d)
{
    (void)item; AppGui *gui = d;
    app_gui_show(gui);
}
static void tray_on_quit(GtkMenuItem *item, gpointer d)
{
    (void)item; (void)d;
    gtk_main_quit();
}

static void on_tray_activate(GtkStatusIcon *icon, gpointer d)
{
    /* Left-click: toggle gallery */
    (void)icon; AppGui *gui = d;
    if (gallery_window_visible(gui->gallery))
        gallery_window_hide(gui->gallery);
    else
        gallery_window_show(gui->gallery);
}

static void on_tray_popup(GtkStatusIcon *icon, guint button,
                           guint activate_time, gpointer d)
{
    (void)icon; AppGui *gui = d;

    GtkWidget *menu = gtk_menu_new();

    /* Status item (non-clickable) */
    GtkWidget *status_item = gtk_menu_item_new();
    GtkWidget *slbl = gtk_label_new(NULL);
    if (wallpaper_engine_state(gui->engine) == WP_STATE_PLAYING &&
        gui->engine->current_file) {
        gchar *base = g_path_get_basename(gui->engine->current_file);
        gchar *m = g_strdup_printf(
            "<span foreground='#27ae60' weight='bold'>● %s</span>", base);
        gtk_label_set_markup(GTK_LABEL(slbl), m);
        g_free(m); g_free(base);
    } else {
        gtk_label_set_markup(GTK_LABEL(slbl),
            "<span foreground='#888888'>● Not playing</span>");
    }
    gtk_label_set_xalign(GTK_LABEL(slbl), 0.0f);
    gtk_container_add(GTK_CONTAINER(status_item), slbl);
    gtk_widget_set_sensitive(status_item, FALSE);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), status_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          gtk_separator_menu_item_new());

    GtkWidget *gallery_item = gtk_menu_item_new_with_label("🎬  Gallery");
    g_signal_connect(gallery_item, "activate",
                     G_CALLBACK(tray_on_show_gallery), gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gallery_item);

    GtkWidget *settings_item = gtk_menu_item_new_with_label("⚙  Settings");
    g_signal_connect(settings_item, "activate",
                     G_CALLBACK(tray_on_show_settings), gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), settings_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(menu),
                          gtk_separator_menu_item_new());

    GtkWidget *quit_item = gtk_menu_item_new_with_label("✕  Quit");
    g_signal_connect(quit_item, "activate", G_CALLBACK(tray_on_quit), gui);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), quit_item);

    gtk_widget_show_all(menu);
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL,
                   gtk_status_icon_position_menu, icon,
                   button, activate_time);
}

static GtkStatusIcon *make_tray(AppGui *gui)
{
    GtkStatusIcon *icon = gtk_status_icon_new_from_icon_name(
        "preferences-desktop-wallpaper");
    gtk_status_icon_set_tooltip_text(icon, "Live Wallpaper Engine");
    gtk_status_icon_set_visible(icon, TRUE);
    g_signal_connect(icon, "activate",   G_CALLBACK(on_tray_activate), gui);
    g_signal_connect(icon, "popup-menu", G_CALLBACK(on_tray_popup),    gui);
    return icon;
}

/* ------------------------------------------------------------------ */
/*  Gallery status change callback                                      */
/* ------------------------------------------------------------------ */
static void on_gallery_status_change(gpointer d)
{
    AppGui *gui = d;
    update_status(gui);
}

static void on_settings_from_gallery(gpointer d)
{
    AppGui *gui = d;
    app_gui_show(gui);
}

/* ------------------------------------------------------------------ */
/*  Row helper                                                          */
/* ------------------------------------------------------------------ */
static GtkWidget *make_row(const char *txt, const char *subtitle, GtkWidget *w)
{
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_margin_start (row, 16);
    gtk_widget_set_margin_end   (row, 16);
    gtk_widget_set_margin_top   (row, 10);
    gtk_widget_set_margin_bottom(row, 10);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_set_hexpand(vbox, TRUE);

    GtkWidget *lbl = gtk_label_new(txt);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(vbox), lbl, FALSE, FALSE, 0);

    if (subtitle) {
        GtkWidget *sub = gtk_label_new(NULL);
        gchar *m = g_strdup_printf("<span foreground='#888888' size='small'>%s</span>",
                                   subtitle);
        gtk_label_set_markup(GTK_LABEL(sub), m);
        g_free(m);
        gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
        gtk_box_pack_start(GTK_BOX(vbox), sub, FALSE, FALSE, 0);
    }

    gtk_box_pack_start(GTK_BOX(row), vbox, TRUE, TRUE, 0);
    gtk_box_pack_end  (GTK_BOX(row), w,    FALSE, FALSE, 0);
    return row;
}

/* ------------------------------------------------------------------ */
/*  Constructor                                                         */
/* ------------------------------------------------------------------ */
AppGui *app_gui_new(WallpaperEngine *engine, GalleryWindow *gallery, AppConfig *config)
{
    AppGui *gui = g_new0(AppGui, 1);
    gui->engine  = engine;
    gui->gallery = gallery;
    gui->config  = config;

    gallery_window_set_status_cb  (gallery, on_gallery_status_change, gui);
    gallery_window_set_settings_cb(gallery, on_settings_from_gallery, gui);

    /* ---- Settings window ---- */
    gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gui->window), "Live Wallpaper — Settings");
    gtk_window_set_default_size(GTK_WINDOW(gui->window), 360, -1);
    gtk_window_set_resizable(GTK_WINDOW(gui->window), FALSE);
    gtk_window_set_position(GTK_WINDOW(gui->window), GTK_WIN_POS_CENTER);
    /* Closing just hides, doesn't quit */
    g_signal_connect(gui->window, "delete-event",
                     G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    GtkWidget *hb = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(hb), "Live Wallpaper");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(hb), "Settings");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hb), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(gui->window), hb);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(gui->window), root);

    /* Status row */
    GtkWidget *sbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start (sbox, 16);
    gtk_widget_set_margin_end   (sbox, 16);
    gtk_widget_set_margin_top   (sbox, 14);
    gtk_widget_set_margin_bottom(sbox, 10);
    gtk_box_pack_start(GTK_BOX(root), sbox, FALSE, FALSE, 0);

    gui->status_label = gtk_label_new(NULL);
    gtk_label_set_xalign(GTK_LABEL(gui->status_label), 0.0f);
    update_status(gui);
    gtk_box_pack_start(GTK_BOX(sbox), gui->status_label, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(root),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    /* Settings list */
    GtkWidget *listbox = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(listbox), GTK_SELECTION_NONE);
    gtk_widget_set_margin_start (listbox, 8);
    gtk_widget_set_margin_end   (listbox, 8);
    gtk_widget_set_margin_top   (listbox, 4);
    gtk_widget_set_margin_bottom(listbox, 4);
    gtk_box_pack_start(GTK_BOX(root), listbox, FALSE, FALSE, 0);

    gui->autostart_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(gui->autostart_switch), config->autostart);
    g_signal_connect(gui->autostart_switch, "notify::active",
                     G_CALLBACK(on_autostart), gui);
    gtk_list_box_insert(GTK_LIST_BOX(listbox),
        make_row("Start on login",
                 "Launch automatically when you sign in",
                 gui->autostart_switch), -1);

    gui->mute_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(gui->mute_switch), config->mute);
    g_signal_connect(gui->mute_switch, "notify::active",
                     G_CALLBACK(on_mute), gui);
    gtk_list_box_insert(GTK_LIST_BOX(listbox),
        make_row("Mute audio",
                 "Silence the wallpaper video",
                 gui->mute_switch), -1);

    gtk_box_pack_start(GTK_BOX(root),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    /* Tray icon */
    gui->tray_icon = make_tray(gui);

    return gui;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */
void app_gui_show(AppGui *gui)
{
    update_status(gui);
    gtk_widget_show_all(gui->window);
    gtk_window_present(GTK_WINDOW(gui->window));
}

void app_gui_show_gallery(AppGui *gui)
{
    gallery_window_show(gui->gallery);
}

void app_gui_run(AppGui *gui)  { (void)gui; gtk_main(); }
void app_gui_free(AppGui *gui)
{
    if (!gui) return;
    if (gui->tray_icon) g_object_unref(gui->tray_icon);
    g_free(gui);
}
