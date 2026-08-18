#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>
#include "wallpaper.h"
#include "gallery.h"
#include "config.h"

typedef struct {
    GtkWidget       *window;
    GtkWidget       *status_label;
    GtkWidget       *autostart_switch;
    GtkWidget       *mute_switch;
    GtkWidget       *start_min_switch;
    WallpaperEngine *engine;
    GalleryWindow   *gallery;
    AppConfig       *config;
    GtkStatusIcon   *tray_icon;
} AppGui;

AppGui *app_gui_new        (WallpaperEngine *engine, GalleryWindow *gallery, AppConfig *config);
void    app_gui_show       (AppGui *gui);
void    app_gui_show_gallery(AppGui *gui);
void    app_gui_run        (AppGui *gui);
void    app_gui_free       (AppGui *gui);

#endif