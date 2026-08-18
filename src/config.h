#ifndef CONFIG_H
#define CONFIG_H

#include <glib.h>

#define APP_NAME     "Live Wallpaper Engine"
#define APP_VERSION  "2.0.0"

typedef struct {
    gchar   *selected_path;
    gboolean autostart;
    gboolean start_minimized;
    gboolean mute;
} AppConfig;

AppConfig *config_load         (void);
void       config_save         (AppConfig *cfg);
void       config_free         (AppConfig *cfg);
gchar     *config_get_path     (const char *filename);
gchar    **gallery_load        (gint *out_count);
void       gallery_save        (gchar **paths, gint count);
void       gallery_add_path    (const gchar *path);
void       gallery_remove_path (const gchar *path);

#endif