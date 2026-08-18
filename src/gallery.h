#ifndef GALLERY_H
#define GALLERY_H

#include <gtk/gtk.h>
#include "wallpaper.h"
#include "config.h"

typedef struct {
    GtkWidget       *window;
    GtkWidget       *flow_box;
    GtkWidget       *status_label;
    GtkWidget       *settings_btn;
    WallpaperEngine *engine;
    AppConfig       *config;
    gchar           *selected_path;
    GtkWidget       *selected_card;
    /* callback: notify settings window of wallpaper status change */
    void           (*on_status_change)(gpointer user_data);
    gpointer         on_status_data;
    /* callback: open settings window when gear icon clicked */
    void           (*on_settings_open)(gpointer user_data);
    gpointer         on_settings_data;
} GalleryWindow;

GalleryWindow *gallery_window_new    (WallpaperEngine *engine, AppConfig *config);
void           gallery_window_show   (GalleryWindow *gw);
void           gallery_window_hide   (GalleryWindow *gw);
void           gallery_window_free   (GalleryWindow *gw);
gboolean       gallery_window_visible(GalleryWindow *gw);
gchar         *gallery_get_selected  (GalleryWindow *gw);

void gallery_window_apply_selected   (GalleryWindow *gw);
void gallery_window_refresh_status   (GalleryWindow *gw);
void gallery_window_set_status_cb    (GalleryWindow *gw,
                                      void (*cb)(gpointer), gpointer data);
void gallery_window_set_settings_cb  (GalleryWindow *gw,
                                      void (*cb)(gpointer), gpointer data);

/* Persistence helpers (implemented in config.c / a separate file) */
void   gallery_add_path   (const gchar *path);
void   gallery_remove_path(const gchar *path);
gchar **gallery_load      (gint *count);

#endif
