#ifndef WALLPAPER_H
#define WALLPAPER_H

#include <gtk/gtk.h>
#include <gst/gst.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>

typedef enum { WP_STATE_STOPPED, WP_STATE_PLAYING } WallpaperState;

typedef struct {
    Display       *display;
    Window         root;
    Window         x_window;
    Window         icons_window;
    GstElement    *pipeline;
    WallpaperState state;
    int            screen_width;
    int            screen_height;
    char          *current_file;
    gulong         bus_watch_id;
    guint          x_io_watch_id;
    guint          poll_timer_id;
    char           saved_bg_uri[1024];
    char           saved_bg_opts[256];
    char           saved_bg_color[64];
} WallpaperEngine;

WallpaperEngine *wallpaper_engine_new      (void);
void             wallpaper_engine_free     (WallpaperEngine *e);
gboolean         wallpaper_engine_set_file (WallpaperEngine *e, const char *filepath);
void             wallpaper_engine_stop     (WallpaperEngine *e);
WallpaperState   wallpaper_engine_state    (WallpaperEngine *e);
gchar           *wallpaper_engine_get_frame(WallpaperEngine *e, const char *filepath);

#endif