#define _POSIX_C_SOURCE 200809L
#include <gtk/gtk.h>
#include <gst/gst.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include "wallpaper.h"
#include "gallery.h"
#include "config.h"
#include "gui.h"

#define LOCK_FILE "live-wallpaper.lock"
#define SHOW_FILE "live-wallpaper.show"

static gchar *get_runtime_file(const char *name)
{
    return g_build_filename(g_get_user_runtime_dir(), name, NULL);
}

static gboolean is_already_running(void)
{
    gchar *lock = get_runtime_file(LOCK_FILE);
    if (!g_file_test(lock, G_FILE_TEST_EXISTS)) {
        g_free(lock);
        return FALSE;
    }

    /* Read the PID and check if that process is actually alive */
    gchar *content = NULL;
    gboolean alive = FALSE;
    if (g_file_get_contents(lock, &content, NULL, NULL)) {
        int pid = atoi(content);
        if (pid > 0 && kill(pid, 0) == 0)
            alive = TRUE;
        g_free(content);
    }

    /* Stale lock — remove it so we can start fresh */
    if (!alive)
        remove(lock);

    g_free(lock);
    return alive;
}

static void write_lock(void)
{
    gchar *lock = get_runtime_file(LOCK_FILE);
    gchar *pid  = g_strdup_printf("%d\n", (int)getpid());
    GError *err = NULL;
    g_file_set_contents(lock, pid, -1, &err);
    if (err) { g_warning("lock: %s", err->message); g_error_free(err); }
    g_free(pid); g_free(lock);
}

static void remove_lock(void)
{
    gchar *f = get_runtime_file(LOCK_FILE); remove(f); g_free(f);
}

static void signal_show(void)
{
    gchar *f = get_runtime_file(SHOW_FILE);
    GError *err = NULL;
    g_file_set_contents(f, "1", -1, &err);
    if (err) g_error_free(err);
    g_free(f);
}

typedef struct { AppGui *gui; GalleryWindow *gallery; } ShowCtx;

static gboolean check_show_signal(gpointer d)
{
    ShowCtx *ctx = d;
    gchar   *sig = get_runtime_file(SHOW_FILE);

    if (g_file_test(sig, G_FILE_TEST_EXISTS)) {
        remove(sig);
        /* Always show the gallery when signalled */
        gallery_window_show(ctx->gallery);
    }
    g_free(sig);
    return G_SOURCE_CONTINUE;
}

int main(int argc, char *argv[])
{
    gst_init(&argc, &argv);
    gtk_init(&argc, &argv);

    AppConfig *config = config_load();

    gboolean has_selected = config->selected_path &&
                            strlen(config->selected_path) > 0 &&
                            g_file_test(config->selected_path, G_FILE_TEST_EXISTS);

    if (is_already_running()) {
        signal_show();
        config_free(config);
        return 0;
    }

    write_lock();

    WallpaperEngine *engine = wallpaper_engine_new();
    if (!engine) {
        remove_lock(); config_free(config); return 1;
    }

    GalleryWindow *gallery = gallery_window_new(engine, config);
    AppGui        *gui     = app_gui_new(engine, gallery, config);

    if (has_selected) {
        /* Auto-apply saved wallpaper silently — only tray icon visible */
        wallpaper_engine_set_file(engine, config->selected_path);
    } else {
        /* No wallpaper selected yet — open gallery so user can pick one */
        gallery_window_show(gallery);
    }

    ShowCtx ctx = { gui, gallery };
    g_timeout_add(500, check_show_signal, &ctx);

    app_gui_run(gui);

    remove_lock();
    gallery_window_free(gallery);
    app_gui_free(gui);
    wallpaper_engine_free(engine);
    config_free(config);
    return 0;
}