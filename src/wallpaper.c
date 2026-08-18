#define _POSIX_C_SOURCE 200809L
#include "wallpaper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <gst/gst.h>
#include <gst/video/videooverlay.h>

static void gsettings_get(const char *key, char *out, size_t sz)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             "gsettings get org.gnome.desktop.background %s 2>/dev/null", key);
    FILE *fp = popen(cmd, "r");
    if (!fp) { out[0]='\0'; return; }
    if (fgets(out, (int)sz, fp)) {
        size_t len = strlen(out);
        if (len && out[len-1]=='\n') out[--len]='\0';
        if (len>=2 && out[0]=='\'' && out[len-1]=='\'') {
            memmove(out, out+1, len-1); out[len-2]='\0';
        }
    } else out[0]='\0';
    pclose(fp);
}

static void gsettings_set(const char *key, const char *val)
{
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "gsettings set org.gnome.desktop.background %s '%s'", key, val);
    int r = system(cmd); (void)r;
}

static void save_bg(WallpaperEngine *e)
{
    gsettings_get("picture-uri",     e->saved_bg_uri,   sizeof(e->saved_bg_uri));
    gsettings_get("picture-options", e->saved_bg_opts,  sizeof(e->saved_bg_opts));
    gsettings_get("primary-color",   e->saved_bg_color, sizeof(e->saved_bg_color));
}

static void set_bg_from_frame(WallpaperEngine *e, const char *filepath)
{
    gchar *frame = wallpaper_engine_get_frame(e, filepath);
    if (!frame) return;

    gchar *uri = g_filename_to_uri(frame, NULL, NULL);
    if (uri) {
        gsettings_set("picture-uri",      uri);
        gsettings_set("picture-uri-dark", uri);
        gsettings_set("picture-options",  "zoom");
        /* Do NOT touch primary-color — leave whatever color the user had */
        g_free(uri);
    }
    g_free(frame);
}

static void restore_bg(WallpaperEngine *e)
{
    if (strlen(e->saved_bg_uri)   > 0) gsettings_set("picture-uri",    e->saved_bg_uri);
    if (strlen(e->saved_bg_opts)  > 0) gsettings_set("picture-options", e->saved_bg_opts);
    if (strlen(e->saved_bg_color) > 0) gsettings_set("primary-color",   e->saved_bg_color);
}

static Window find_ding(Display *dpy, Window root, Window exclude)
{
    Window dummy, *ch = NULL; unsigned int n = 0;
    if (!XQueryTree(dpy, root, &dummy, &dummy, &ch, &n)) return None;
    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE",         False);
    Atom tp_desk = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    Window found = None;
    for (unsigned int i = 0; i < n; i++) {
        if (ch[i] == exclude) continue;
        Atom at; int af; unsigned long ni, ba; unsigned char *prop = NULL;
        if (XGetWindowProperty(dpy, ch[i], wm_type, 0, 1, False, XA_ATOM,
                               &at, &af, &ni, &ba, &prop) == Success && prop) {
            if (*(Atom*)prop == tp_desk) { found = ch[i]; XFree(prop); break; }
            XFree(prop);
        }
    }
    if (ch) XFree(ch);
    return found;
}

static void ensure_stacking(WallpaperEngine *e)
{
    if (e->icons_window == None)
        e->icons_window = find_ding(e->display, e->root, e->x_window);
    if (e->icons_window != None)
        XRaiseWindow(e->display, e->icons_window);
    XLowerWindow(e->display, e->x_window);
    XFlush(e->display);
}

static gboolean poll_cb(gpointer d)
{
    WallpaperEngine *e = d;
    if (e->state == WP_STATE_PLAYING) ensure_stacking(e);
    return G_SOURCE_CONTINUE;
}

static gboolean x_event_cb(GIOChannel *src, GIOCondition cond, gpointer d)
{
    (void)src; (void)cond;
    WallpaperEngine *e = d;
    gboolean any = FALSE;
    while (XPending(e->display) > 0) {
        XEvent ev;
        XNextEvent(e->display, &ev);
        /* Watch for _NET_SHOWING_DESKTOP changes (Activities open/close) */
        if (ev.type == PropertyNotify) {
            Atom showing = XInternAtom(e->display, "_NET_SHOWING_DESKTOP", False);
            if (ev.xproperty.atom == showing && e->state == WP_STATE_PLAYING) {
                /* Activities just closed — re-assert our stacking */
                g_timeout_add(200, (GSourceFunc)ensure_stacking, e);
            }
        }
        any = TRUE;
    }
    if (any && e->state == WP_STATE_PLAYING) ensure_stacking(e);
    return TRUE;
}

static Window make_window(Display *dpy, Window root, int *ow, int *oh)
{
    int scr = DefaultScreen(dpy);
    *ow = DisplayWidth(dpy, scr); *oh = DisplayHeight(dpy, scr);

    XSetWindowAttributes a; memset(&a, 0, sizeof(a));
    a.background_pixel = BlackPixel(dpy, scr);
    a.event_mask = 0;

    Window win = XCreateWindow(dpy, root, 0, 0, *ow, *oh, 0,
        DefaultDepth(dpy, scr), InputOutput, DefaultVisual(dpy, scr),
        CWBackPixel | CWEventMask, &a);

    XShapeCombineRectangles(dpy, win, ShapeInput, 0, 0, NULL, 0, ShapeSet, YXBanded);

    Atom wm_type = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE",         False);
    Atom tp_desk = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DESKTOP", False);
    XChangeProperty(dpy, win, wm_type, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)&tp_desk, 1);

    Atom wm_st   = XInternAtom(dpy, "_NET_WM_STATE",              False);
    Atom st_task = XInternAtom(dpy, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom st_page = XInternAtom(dpy, "_NET_WM_STATE_SKIP_PAGER",   False);
    Atom st_stk  = XInternAtom(dpy, "_NET_WM_STATE_STICKY",       False);
    Atom sts[3]  = {st_task, st_page, st_stk};
    XChangeProperty(dpy, win, wm_st, XA_ATOM, 32,
                    PropModeReplace, (unsigned char*)sts, 3);

    Atom nd = XInternAtom(dpy, "_NET_WM_DESKTOP", False);
    unsigned long dv = 0xFFFFFFFFUL;
    XChangeProperty(dpy, win, nd, XA_CARDINAL, 32,
                    PropModeReplace, (unsigned char*)&dv, 1);

    XMapWindow(dpy, win); XFlush(dpy); g_usleep(300000);
    return win;
}

static gboolean bus_cb(GstBus *bus, GstMessage *msg, gpointer d)
{
    WallpaperEngine *e = d; (void)bus;
    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_EOS:
        gst_element_seek_simple(e->pipeline, GST_FORMAT_TIME,
            GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT, 0);
        gst_element_set_state(e->pipeline, GST_STATE_PLAYING);
        break;
    case GST_MESSAGE_ERROR: {
        GError *err=NULL; gchar *dbg=NULL;
        gst_message_parse_error(msg, &err, &dbg);
        g_warning("[GST] %s", err->message);
        g_error_free(err); g_free(dbg); break;
    }
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(e->pipeline)) {
            GstState os, ns, p;
            gst_message_parse_state_changed(msg, &os, &ns, &p);
            if (ns == GST_STATE_PLAYING) ensure_stacking(e);
        }
        break;
    default: break;
    }
    return TRUE;
}

static void destroy_pipeline(WallpaperEngine *e)
{
    if (!e->pipeline) return;
    GstBus *bus = gst_element_get_bus(e->pipeline);
    if (bus) { gst_bus_remove_watch(bus); gst_object_unref(bus); }
    e->bus_watch_id = 0;
    gst_element_set_state(e->pipeline, GST_STATE_NULL);
    gst_object_unref(e->pipeline); e->pipeline = NULL;
}

static gboolean build_pipeline(WallpaperEngine *e, const char *path)
{
    destroy_pipeline(e);
    GError *uerr = NULL;
    gchar *uri = g_filename_to_uri(path, NULL, &uerr);
    if (!uri) { if (uerr) g_error_free(uerr); uri = g_strdup_printf("file://%s", path); }

    const char *sinks[] = {"xvimagesink", "ximagesink", NULL};
    for (int si = 0; sinks[si]; si++) {
        gchar *ps = g_strdup_printf(
            "uridecodebin uri=\"%s\" name=src "
            "src. ! queue max-size-buffers=200 ! videoconvert ! "
            "video/x-raw ! %s name=vsink force-aspect-ratio=false sync=true",
            uri, sinks[si]);
        GError *err = NULL;
        e->pipeline = gst_parse_launch(ps, &err); g_free(ps);
        if (!err && e->pipeline) {
            GstElement *vsink = gst_bin_get_by_name(GST_BIN(e->pipeline), "vsink");
            if (vsink && GST_IS_VIDEO_OVERLAY(vsink)) {
                gst_video_overlay_set_window_handle(
                    GST_VIDEO_OVERLAY(vsink), (guintptr)e->x_window);
                gst_video_overlay_set_render_rectangle(
                    GST_VIDEO_OVERLAY(vsink), 0, 0, e->screen_width, e->screen_height);
                gst_object_unref(vsink);
            }
            GstBus *bus = gst_element_get_bus(e->pipeline);
            e->bus_watch_id = gst_bus_add_watch(bus, bus_cb, e);
            gst_object_unref(bus);
            g_free(uri); return TRUE;
        }
        if (err) g_error_free(err);
        if (e->pipeline) { gst_object_unref(e->pipeline); e->pipeline = NULL; }
    }
    g_free(uri); return FALSE;
}

/* Extract a frame at ~5 seconds using ffmpeg (reliable) and cache it.
 * Returns allocated path to the JPEG, or NULL on failure. */
gchar *wallpaper_engine_get_frame(WallpaperEngine *e, const char *filepath)
{
    (void)e;
    gchar *thumb_dir = g_build_filename(g_get_user_cache_dir(),
                                        "live-wallpaper", "thumbs", NULL);
    g_mkdir_with_parents(thumb_dir, 0755);

    gchar *base  = g_path_get_basename(filepath);
    gchar *tname = g_strdup_printf("%s_bg.jpg", base);
    gchar *tpath = g_build_filename(thumb_dir, tname, NULL);
    g_free(base); g_free(tname); g_free(thumb_dir);

    /* Return cached copy if it exists */
    if (g_file_test(tpath, G_FILE_TEST_EXISTS)) return tpath;

    /* Use ffmpeg: seek to 5s, grab 1 frame at full resolution */
    gchar *cmd = g_strdup_printf(
        "ffmpeg -y -ss 5 -i %s -vframes 1 -q:v 2 %s 2>/dev/null",
        g_shell_quote(filepath), g_shell_quote(tpath));

    int ret = system(cmd);
    g_free(cmd);

    /* If 5s seek failed (short video), try from the start */
    if (ret != 0 || !g_file_test(tpath, G_FILE_TEST_EXISTS)) {
        gchar *cmd2 = g_strdup_printf(
            "ffmpeg -y -i %s -vframes 1 -q:v 2 %s 2>/dev/null",
            g_shell_quote(filepath), g_shell_quote(tpath));
        system(cmd2);
        g_free(cmd2);
        (void)ret;
    }

    if (!g_file_test(tpath, G_FILE_TEST_EXISTS)) {
        g_free(tpath); return NULL;
    }
    return tpath;
}

WallpaperEngine *wallpaper_engine_new(void)
{
    WallpaperEngine *e = calloc(1, sizeof(*e));
    if (!e) return NULL;
    e->display = XOpenDisplay(NULL);
    if (!e->display) { free(e); return NULL; }
    e->root = DefaultRootWindow(e->display);
    save_bg(e);
    /* Don't blank the bg yet — caller will call set_file which sets the frame */
    e->x_window = make_window(e->display, e->root, &e->screen_width, &e->screen_height);
    e->icons_window = find_ding(e->display, e->root, e->x_window);
    e->state = WP_STATE_STOPPED;
    ensure_stacking(e);
    XSelectInput(e->display, e->root,
                 SubstructureNotifyMask | PropertyChangeMask);
    XFlush(e->display);
    int fd = ConnectionNumber(e->display);
    GIOChannel *chan = g_io_channel_unix_new(fd);
    e->x_io_watch_id = g_io_add_watch(chan, G_IO_IN, x_event_cb, e);
    g_io_channel_unref(chan);
    e->poll_timer_id = g_timeout_add(150, poll_cb, e);
    return e;
}

void wallpaper_engine_free(WallpaperEngine *e)
{
    if (!e) return;
    if (e->x_io_watch_id) g_source_remove(e->x_io_watch_id);
    if (e->poll_timer_id) g_source_remove(e->poll_timer_id);
    wallpaper_engine_stop(e);
    restore_bg(e);
    if (e->display) { XDestroyWindow(e->display, e->x_window); XCloseDisplay(e->display); }
    free(e->current_file); free(e);
}

gboolean wallpaper_engine_set_file(WallpaperEngine *e, const char *path)
{
    if (!e || !path) return FALSE;

    /* Set the frame FIRST so desktop never goes black */
    set_bg_from_frame(e, path);

    if (!build_pipeline(e, path)) return FALSE;
    free(e->current_file); e->current_file = strdup(path);

    GstStateChangeReturn r = gst_element_set_state(e->pipeline, GST_STATE_PLAYING);
    if (r == GST_STATE_CHANGE_FAILURE) {
        destroy_pipeline(e); e->state = WP_STATE_STOPPED; return FALSE;
    }

    /* Wait up to 4s for PLAYING state so the video window is actually rendering */
    gst_element_get_state(e->pipeline, NULL, NULL, 4 * GST_SECOND);

    /* Keep the frame as GNOME bg — it shows in Activities/App drawer
     * and acts as a fallback. The X window sits on top while on desktop. */
    /* (picture-uri stays set — we do NOT clear it) */

    e->state = WP_STATE_PLAYING; ensure_stacking(e); return TRUE;
}

void wallpaper_engine_stop(WallpaperEngine *e)
{
    if (!e) return;
    destroy_pipeline(e); e->state = WP_STATE_STOPPED;
    if (e->display) {
        int scr = DefaultScreen(e->display);
        XSetWindowBackground(e->display, e->x_window, BlackPixel(e->display, scr));
        XClearWindow(e->display, e->x_window); XFlush(e->display);
    }
    /* Show the last frame as desktop bg instead of solid black */
    if (e->current_file)
        set_bg_from_frame(e, e->current_file);
}

WallpaperState wallpaper_engine_state(WallpaperEngine *e)
{ return e ? e->state : WP_STATE_STOPPED; }