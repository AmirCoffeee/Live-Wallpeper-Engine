#define _POSIX_C_SOURCE 200809L
#include "config.h"
#include <string.h>

gchar *config_get_path(const char *filename)
{
    return g_build_filename(g_get_user_config_dir(),
                            "live-wallpaper", filename, NULL);
}

static void ensure_dir(void)
{
    gchar *dir = g_build_filename(g_get_user_config_dir(),
                                  "live-wallpaper", NULL);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
}

AppConfig *config_load(void)
{
    AppConfig *cfg    = g_new0(AppConfig, 1);
    cfg->mute         = TRUE;
    cfg->autostart    = FALSE;
    cfg->start_minimized = FALSE;
    cfg->selected_path   = NULL;

    gchar    *path = config_get_path("config.ini");
    GKeyFile *kf   = g_key_file_new();

    if (g_key_file_load_from_file(kf, path, G_KEY_FILE_NONE, NULL)) {
        cfg->autostart       = g_key_file_get_boolean(kf, "General", "autostart",       NULL);
        cfg->start_minimized = g_key_file_get_boolean(kf, "General", "start_minimized", NULL);
        cfg->mute            = g_key_file_get_boolean(kf, "General", "mute",            NULL);
        cfg->selected_path   = g_key_file_get_string (kf, "General", "selected_path",   NULL);
    }

    g_key_file_free(kf);
    g_free(path);
    return cfg;
}

void config_save(AppConfig *cfg)
{
    ensure_dir();
    gchar    *path = config_get_path("config.ini");
    GKeyFile *kf   = g_key_file_new();

    g_key_file_set_boolean(kf, "General", "autostart",       cfg->autostart);
    g_key_file_set_boolean(kf, "General", "start_minimized", cfg->start_minimized);
    g_key_file_set_boolean(kf, "General", "mute",            cfg->mute);
    if (cfg->selected_path)
        g_key_file_set_string(kf, "General", "selected_path", cfg->selected_path);

    GError *err = NULL;
    g_key_file_save_to_file(kf, path, &err);
    if (err) { g_warning("config_save: %s", err->message); g_error_free(err); }
    g_key_file_free(kf);
    g_free(path);
}

void config_free(AppConfig *cfg)
{
    if (!cfg) return;
    g_free(cfg->selected_path);
    g_free(cfg);
}

gchar **gallery_load(gint *out_count)
{
    *out_count = 0;
    gchar *path    = config_get_path("gallery.txt");
    gchar *content = NULL;

    if (!g_file_get_contents(path, &content, NULL, NULL)) {
        g_free(path); return NULL;
    }

    gchar    **lines = g_strsplit(content, "\n", -1);
    GPtrArray *arr   = g_ptr_array_new();
    g_free(content); g_free(path);

    for (gint i = 0; lines[i]; i++) {
        g_strstrip(lines[i]);
        if (strlen(lines[i]) > 0)
            g_ptr_array_add(arr, g_strdup(lines[i]));
    }
    g_strfreev(lines);

    *out_count = (gint)arr->len;
    g_ptr_array_add(arr, NULL);
    return (gchar **)g_ptr_array_free(arr, FALSE);
}

void gallery_save(gchar **paths, gint count)
{
    ensure_dir();
    gchar   *file = config_get_path("gallery.txt");
    GString *buf  = g_string_new(NULL);
    for (gint i = 0; i < count; i++) {
        g_string_append(buf, paths[i]);
        g_string_append_c(buf, '\n');
    }
    GError *err = NULL;
    g_file_set_contents(file, buf->str, buf->len, &err);
    if (err) { g_warning("gallery_save: %s", err->message); g_error_free(err); }
    g_string_free(buf, TRUE);
    g_free(file);
}

void gallery_add_path(const gchar *path)
{
    gint   count = 0;
    gchar **paths = gallery_load(&count);
    GPtrArray *arr = g_ptr_array_new();

    if (paths) {
        for (gint i = 0; i < count; i++)
            if (g_strcmp0(paths[i], path) != 0)
                g_ptr_array_add(arr, g_strdup(paths[i]));
        g_strfreev(paths);
    }
    g_ptr_array_add(arr, g_strdup(path));
    gallery_save((gchar **)arr->pdata, (gint)arr->len);
    g_ptr_array_free(arr, TRUE);
}

void gallery_remove_path(const gchar *path)
{
    gint   count = 0;
    gchar **paths = gallery_load(&count);
    if (!paths) return;
    GPtrArray *arr = g_ptr_array_new();
    for (gint i = 0; i < count; i++)
        if (g_strcmp0(paths[i], path) != 0)
            g_ptr_array_add(arr, g_strdup(paths[i]));
    g_strfreev(paths);
    gallery_save((gchar **)arr->pdata, (gint)arr->len);
    g_ptr_array_free(arr, TRUE);
}