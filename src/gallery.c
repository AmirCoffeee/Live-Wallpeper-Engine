#define _POSIX_C_SOURCE 200809L
#include "gallery.h"
#include <string.h>

/* Card size — 16:9. Width is fixed; height is 1/3 of window height (set dynamically) */
#define CARD_W 220
#define CARD_H 124   /* fallback before window is shown */

/* ------------------------------------------------------------------ */
/*  Dynamic card sizing — called when gallery window resizes           */
/* ------------------------------------------------------------------ */
typedef struct { GalleryWindow *gw; } ResizeCtx;

static void on_scroll_size_allocate(GtkWidget *w, GdkRectangle *alloc, gpointer d)
{
    (void)w;
    GalleryWindow *gw = d;

    /* 4 columns, 3 rows visible — each card = 1/3 height, 1/4 width
     * subtract margins (10px each side = 20) and spacing between cards (5px margin*2 = 10 per card) */
    int cols = 4;
    int rows = 3;

    int total_w = alloc->width  - 20; /* 10px margin each side */
    int total_h = alloc->height - 20;

    int card_w = (total_w  - cols * 10) / cols;  /* 10px = margin*2 per card */
    int card_h = (total_h  - rows * 10) / rows;

    /* maintain 16:9 — use whichever constraint is tighter */
    int card_h_from_w = card_w * 9 / 16;
    if (card_h_from_w < card_h) card_h = card_h_from_w;
    else                        card_w = card_h * 16 / 9;

    card_w = MAX(80,  card_w);
    card_h = MAX(45,  card_h);

    GList *children = gtk_container_get_children(GTK_CONTAINER(gw->flow_box));
    for (GList *l = children; l; l = l->next) {
        GtkWidget *fbc  = GTK_WIDGET(l->data);
        GList     *inner = gtk_container_get_children(GTK_CONTAINER(fbc));
        if (!inner) continue;
        GtkWidget *card = GTK_WIDGET(inner->data);
        gtk_widget_set_size_request(card, card_w, card_h);

        /* DrawingArea is inside Overlay inside EventBox */
        GList *ov_children = gtk_container_get_children(GTK_CONTAINER(card));
        if (ov_children) {
            GList *da_list = gtk_container_get_children(
                GTK_CONTAINER(ov_children->data));
            if (da_list)
                gtk_widget_set_size_request(GTK_WIDGET(da_list->data),
                                            card_w, card_h);
            g_list_free(da_list);
            g_list_free(ov_children);
        }
        g_list_free(inner);
    }
    g_list_free(children);
}

typedef struct {
    GalleryWindow *gw;
    gchar         *path;
    GtkWidget     *card;
} CardData;

static void card_data_free(gpointer p)
{
    CardData *cd = p;
    g_free(cd->path);
    g_free(cd);
}

static void apply_css(void)
{
    static gboolean done = FALSE;
    if (done) return;
    done = TRUE;

    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p,
        "window { background-color: #181818; }"

        /* Strip all GTK FlowBoxChild decoration */
        "flowboxchild,"
        "flowboxchild:selected,"
        "flowboxchild:focus,"
        "flowboxchild:hover {"
        "  padding: 0; margin: 0;"
        "  background: transparent;"
        "  border: none; outline: none; box-shadow: none;"
        "}"

        /* Card outer border — rounded corners */
        ".gallery-card {"
        "  border-radius: 6px;"
        "  border: 2px solid transparent;"
        "  margin: 4px;"
        "  background: #1a1a1a;"
        "  padding: 0;"
        "}"
        ".gallery-card:hover { border-color: #5e81f4; }"
        ".gallery-selected {"
        "  border-radius: 6px;"
        "  border: 2px solid #5e81f4;"
        "  margin: 4px;"
        "  background: #1a1a1a;"
        "  padding: 0;"
        "}"

        /* Make sure the image itself has no margin/padding */
        ".gallery-card image,"
        ".gallery-selected image {"
        "  margin: 0; padding: 0;"
        "}"

        /* Gradient title bar overlaid at bottom of image */
        ".card-title-bar {"
        "  background: linear-gradient(to top,"
        "    rgba(0,0,0,0.80) 0%, rgba(0,0,0,0) 100%);"
        "  padding: 20px 6px 4px 6px;"
        "}"
        ".card-label {"
        "  color: #ffffff; font-size: 10px;"
        "}"

        /* Delete button top-right */
        ".del-btn {"
        "  background: rgba(0,0,0,0.55); border: none;"
        "  border-radius: 4px;"
        "  padding: 1px; min-width: 0; min-height: 0;"
        "  color: #ccc;"
        "}"
        ".del-btn:hover { background: rgba(180,30,30,0.85); color: white; }"

        /* Bottom status bar */
        ".bottom-bar {"
        "  background-color: #111111;"
        "  border-top: 1px solid #2a2a2a;"
        "  padding: 7px 12px;"
        "}"
        ".status-label { font-size: 12px; color: #aaaaaa; }"

        /* Settings gear button */
        ".settings-btn {"
        "  background: transparent; border: none;"
        "  padding: 2px 6px; color: #888; min-width: 0;"
        "}"
        ".settings-btn:hover { color: #5e81f4; }"

        /* Fix GtkSwitch colors in dark theme */
        "switch {"
        "  background: #3a3a3a;"
        "  border: 1px solid #555;"
        "  border-radius: 14px;"
        "}"
        "switch:checked {"
        "  background: #3584e4;"
        "  border-color: #2a74d4;"
        "}"
        "switch slider {"
        "  background: #ffffff;"
        "  border-radius: 12px;"
        "  min-width: 24px; min-height: 24px;"
        "  margin: 2px;"
        "}",
        -1, NULL);

    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}

static void set_card_style(GtkWidget *card, gboolean sel)
{
    GtkStyleContext *ctx = gtk_widget_get_style_context(card);
    if (sel) {
        gtk_style_context_add_class   (ctx, "gallery-selected");
        gtk_style_context_remove_class(ctx, "gallery-card");
    } else {
        gtk_style_context_add_class   (ctx, "gallery-card");
        gtk_style_context_remove_class(ctx, "gallery-selected");
    }
}

/* ------------------------------------------------------------------ */
/*  Status helpers                                                      */
/* ------------------------------------------------------------------ */
static void refresh_status(GalleryWindow *gw)
{
    if (wallpaper_engine_state(gw->engine) == WP_STATE_PLAYING &&
        gw->engine->current_file) {
        gchar *base = g_path_get_basename(gw->engine->current_file);
        gchar *m = g_strdup_printf(
            "<span foreground='#27ae60' weight='bold'>● Live: %s</span>", base);
        gtk_label_set_markup(GTK_LABEL(gw->status_label), m);
        g_free(m); g_free(base);
    } else {
        gtk_label_set_markup(GTK_LABEL(gw->status_label),
            "<span foreground='#7f8c8d'>● No wallpaper active</span>");
    }
}

static void do_apply(GalleryWindow *gw, const gchar *path)
{
    gboolean ok = wallpaper_engine_set_file(gw->engine, path);
    if (ok) {
        gchar *base   = g_path_get_basename(path);
        gchar *markup = g_strdup_printf(
            "<span foreground='#27ae60' weight='bold'>● Live: %s</span>", base);
        gtk_label_set_markup(GTK_LABEL(gw->status_label), markup);
        g_free(markup); g_free(base);
    } else {
        gtk_label_set_markup(GTK_LABEL(gw->status_label),
            "<span foreground='#e74c3c' weight='bold'>● Failed to start</span>");
    }
    if (gw->on_status_change)
        gw->on_status_change(gw->on_status_data);
}

void gallery_window_apply_selected(GalleryWindow *gw)
{
    if (gw->selected_path)
        do_apply(gw, gw->selected_path);
}

/* Called whenever the settings window changes something */
void gallery_window_refresh_status(GalleryWindow *gw)
{
    if (gw && gw->status_label)
        refresh_status(gw);
}

/* ------------------------------------------------------------------ */
/*  Card click                                                          */
/* ------------------------------------------------------------------ */
static gboolean on_card_click(GtkWidget *card, GdkEventButton *ev, gpointer d)
{
    CardData      *cd = d;
    GalleryWindow *gw = cd->gw;

    if (ev->type == GDK_BUTTON_PRESS && ev->button == 1) {
        if (gw->selected_card && gw->selected_card != card)
            set_card_style(gw->selected_card, FALSE);
        gw->selected_card = card;
        set_card_style(card, TRUE);
        g_free(gw->selected_path);
        gw->selected_path = g_strdup(cd->path);
        g_free(gw->config->selected_path);
        gw->config->selected_path = g_strdup(cd->path);
        config_save(gw->config);
        do_apply(gw, cd->path);
    }
    return FALSE;
}

static void on_del(GtkWidget *btn, gpointer d)
{
    (void)btn;
    CardData      *cd = d;
    GalleryWindow *gw = cd->gw;

    gallery_remove_path(cd->path);

    if (g_strcmp0(gw->selected_path, cd->path) == 0) {
        g_free(gw->selected_path);          gw->selected_path  = NULL;
        gw->selected_card = NULL;
        g_free(gw->config->selected_path);  gw->config->selected_path = NULL;
        config_save(gw->config);
        gtk_label_set_markup(GTK_LABEL(gw->status_label),
            "<span foreground='#7f8c8d'>● No wallpaper selected</span>");
    }

    GtkWidget *item = gtk_widget_get_parent(cd->card);
    if (item) gtk_widget_destroy(item);
}

/* ------------------------------------------------------------------ */
/*  Thumbnail — DrawingArea that fills the whole card via cairo        */
/* ------------------------------------------------------------------ */
typedef struct {
    GdkPixbuf *pixbuf;   /* scaled thumb, owned by this struct */
} ThumbData;

static void thumb_data_free(gpointer p)
{
    ThumbData *td = p;
    if (td->pixbuf) g_object_unref(td->pixbuf);
    g_free(td);
}

static gboolean on_thumb_draw(GtkWidget *da, cairo_t *cr, gpointer d)
{
    ThumbData *td = d;
    int w = gtk_widget_get_allocated_width(da);
    int h = gtk_widget_get_allocated_height(da);
    double r = 6.0; /* corner radius */

    /* Clip to rounded rectangle */
    cairo_new_sub_path(cr);
    cairo_arc(cr, r,   r,   r, G_PI,       -G_PI/2);
    cairo_arc(cr, w-r, r,   r, -G_PI/2,    0);
    cairo_arc(cr, w-r, h-r, r, 0,           G_PI/2);
    cairo_arc(cr, r,   h-r, r, G_PI/2,     G_PI);
    cairo_close_path(cr);
    cairo_clip(cr);

    /* Dark background */
    cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
    cairo_paint(cr);

    if (!td->pixbuf) return FALSE;

    int pw = gdk_pixbuf_get_width(td->pixbuf);
    int ph = gdk_pixbuf_get_height(td->pixbuf);

    /* Cover scale */
    double sx = (double)w / pw;
    double sy = (double)h / ph;
    double s  = (sx > sy) ? sx : sy;

    int dw = (int)(pw * s);
    int dh = (int)(ph * s);
    int ox = (w - dw) / 2;
    int oy = (h - dh) / 2;

    cairo_translate(cr, ox, oy);
    cairo_scale(cr, s, s);
    gdk_cairo_set_source_pixbuf(cr, td->pixbuf, 0, 0);
    cairo_paint(cr);
    return FALSE;
}

static GtkWidget *make_thumb_widget(const char *path, WallpaperEngine *engine,
                                    int card_w, int card_h)
{
    ThumbData *td = g_new0(ThumbData, 1);

    gchar *tp = wallpaper_engine_get_frame(engine, path);
    if (tp) {
        GdkPixbuf *raw = gdk_pixbuf_new_from_file(tp, NULL);
        g_free(tp);
        if (raw) td->pixbuf = raw;
    }

    GtkWidget *da = gtk_drawing_area_new();
    gtk_widget_set_size_request(da, card_w, card_h);
    g_object_set_data_full(G_OBJECT(da), "thumb", td, thumb_data_free);
    g_signal_connect(da, "draw", G_CALLBACK(on_thumb_draw), td);
    return da;
}

/* ------------------------------------------------------------------ */
/*  Card widget                                                         */
/* ------------------------------------------------------------------ */
static GtkWidget *make_card(GalleryWindow *gw, const gchar *path)
{
    CardData *cd = g_new0(CardData, 1);
    cd->gw   = gw;
    cd->path = g_strdup(path);

    /* Initial size: assume 860px wide, 460px content height (window 560 - header 60 - bottom 40) */
    int init_w = (860 - 20 - 4*10) / 4;   /* ~195 */
    int init_h = init_w * 9 / 16;          /* ~110 */

    GtkWidget *card = gtk_event_box_new();
    cd->card = card;
    set_card_style(card, FALSE);
    gtk_widget_set_size_request(card, init_w, init_h);
    g_object_set_data_full(G_OBJECT(card), "cd", cd, card_data_free);
    g_signal_connect(card, "button-press-event", G_CALLBACK(on_card_click), cd);

    /* Overlay: drawing area fills the whole card */
    GtkWidget *overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(card), overlay);

    /* Background: DrawingArea renders the thumbnail filling the whole card */
    GtkWidget *thumb = make_thumb_widget(path, gw->engine, init_w, init_h);
    gtk_container_add(GTK_CONTAINER(overlay), thumb);

    /* Title gradient at bottom */
    GtkWidget *title_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(title_bar),
                                "card-title-bar");
    gtk_widget_set_halign(title_bar, GTK_ALIGN_FILL);
    gtk_widget_set_valign(title_bar, GTK_ALIGN_END);
    gtk_widget_set_hexpand(title_bar, TRUE);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), title_bar);
    gtk_overlay_set_overlay_pass_through(GTK_OVERLAY(overlay), title_bar, TRUE);

    gchar *base = g_path_get_basename(path);
    GtkWidget *lbl = gtk_label_new(base);
    g_free(base);
    gtk_label_set_ellipsize(GTK_LABEL(lbl), PANGO_ELLIPSIZE_END);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.5f);
    gtk_widget_set_hexpand(lbl, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(lbl), "card-label");
    gtk_box_pack_start(GTK_BOX(title_bar), lbl, TRUE, TRUE, 0);

    /* Delete button top-right */
    GtkWidget *del = gtk_button_new_from_icon_name("window-close-symbolic",
                                                    GTK_ICON_SIZE_MENU);
    gtk_style_context_add_class(gtk_widget_get_style_context(del), "del-btn");
    gtk_widget_set_halign(del, GTK_ALIGN_END);
    gtk_widget_set_valign(del, GTK_ALIGN_START);
    gtk_widget_set_margin_top(del, 4);
    gtk_widget_set_margin_end(del, 4);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), del);
    g_signal_connect(del, "clicked", G_CALLBACK(on_del), cd);

    if (g_strcmp0(path, gw->config->selected_path) == 0) {
        set_card_style(card, TRUE);
        gw->selected_card = card;
        g_free(gw->selected_path);
        gw->selected_path = g_strdup(path);
    }

    gtk_widget_show_all(card);
    return card;
}

/* ------------------------------------------------------------------ */
/*  Reload                                                              */
/* ------------------------------------------------------------------ */
static void reload_gallery(GalleryWindow *gw)
{
    GList *ch = gtk_container_get_children(GTK_CONTAINER(gw->flow_box));
    for (GList *l = ch; l; l = l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);

    gint   count = 0;
    gchar **paths = gallery_load(&count);
    if (!paths) return;
    for (gint i = 0; i < count; i++) {
        GtkWidget *card = make_card(gw, paths[i]);
        gtk_flow_box_insert(GTK_FLOW_BOX(gw->flow_box), card, -1);
    }
    g_strfreev(paths);
}

/* ------------------------------------------------------------------ */
/*  Add button                                                          */
/* ------------------------------------------------------------------ */
static void on_add(GtkWidget *w, gpointer d)
{
    (void)w;
    GalleryWindow *gw = d;

    GtkWidget *dlg = gtk_file_chooser_dialog_new(
        "Add Video", GTK_WINDOW(gw->window),
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Add",    GTK_RESPONSE_ACCEPT, NULL);
    gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dlg), TRUE);

    GtkFileFilter *fv = gtk_file_filter_new();
    gtk_file_filter_set_name(fv, "Video Files");
    gtk_file_filter_add_mime_type(fv, "video/mp4");
    gtk_file_filter_add_mime_type(fv, "video/x-matroska");
    gtk_file_filter_add_mime_type(fv, "video/webm");
    gtk_file_filter_add_mime_type(fv, "video/x-msvideo");
    gtk_file_filter_add_mime_type(fv, "video/quicktime");
    gtk_file_filter_add_mime_type(fv, "image/gif");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dlg), fv);

    if (gtk_dialog_run(GTK_DIALOG(dlg)) == GTK_RESPONSE_ACCEPT) {
        GSList *files = gtk_file_chooser_get_filenames(GTK_FILE_CHOOSER(dlg));
        for (GSList *l = files; l; l = l->next) {
            gallery_add_path((gchar *)l->data);
            GtkWidget *card = make_card(gw, (gchar *)l->data);
            gtk_flow_box_insert(GTK_FLOW_BOX(gw->flow_box), card, -1);
        }
        g_slist_free_full(files, g_free);
    }
    gtk_widget_destroy(dlg);
}

/* ------------------------------------------------------------------ */
/*  Settings button callback (set by main after gui is created)        */
/* ------------------------------------------------------------------ */
static void on_settings_btn(GtkWidget *w, gpointer d)
{
    (void)w;
    GalleryWindow *gw = d;
    if (gw->on_settings_open)
        gw->on_settings_open(gw->on_settings_data);
}

/* ------------------------------------------------------------------ */
/*  Constructor                                                         */
/* ------------------------------------------------------------------ */
GalleryWindow *gallery_window_new(WallpaperEngine *engine, AppConfig *config)
{
    apply_css();

    GalleryWindow *gw = g_new0(GalleryWindow, 1);
    gw->engine = engine;
    gw->config = config;
    gw->selected_path = config->selected_path ? g_strdup(config->selected_path) : NULL;

    gw->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(gw->window), "Live Wallpaper Engine");
    gtk_window_set_default_size(GTK_WINDOW(gw->window), 860, 560);
    /* Top-left corner */
    gtk_window_set_position(GTK_WINDOW(gw->window), GTK_WIN_POS_NONE);
    gtk_window_move(GTK_WINDOW(gw->window), 0, 0);
    g_signal_connect(gw->window, "delete-event",
                     G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    GtkWidget *hb = gtk_header_bar_new();
    gtk_header_bar_set_title(GTK_HEADER_BAR(hb), "Live Wallpaper Engine");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(hb), "Choose your wallpaper");
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(hb), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(gw->window), hb);

    GtkWidget *add_btn = gtk_button_new_with_label("+ Add");
    gtk_style_context_add_class(gtk_widget_get_style_context(add_btn),
                                "suggested-action");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(hb), add_btn);
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add), gw);

    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(gw->window), root);

    /* Scrollable flow box */
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_box_pack_start(GTK_BOX(root), scroll, TRUE, TRUE, 0);
    /* Resize cards when window resizes */
    g_signal_connect(scroll, "size-allocate",
                     G_CALLBACK(on_scroll_size_allocate), gw);

    gw->flow_box = gtk_flow_box_new();
    gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(gw->flow_box), GTK_SELECTION_NONE);
    gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(gw->flow_box), FALSE);
    gtk_flow_box_set_min_children_per_line(GTK_FLOW_BOX(gw->flow_box), 4);
    gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(gw->flow_box), 4);
    gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(gw->flow_box), 2);
    gtk_flow_box_set_row_spacing   (GTK_FLOW_BOX(gw->flow_box), 2);
    gtk_widget_set_margin_start (gw->flow_box, 8);
    gtk_widget_set_margin_end   (gw->flow_box, 8);
    gtk_widget_set_margin_top   (gw->flow_box, 8);
    gtk_widget_set_margin_bottom(gw->flow_box, 8);
    gtk_widget_set_halign(gw->flow_box, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(gw->flow_box, TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), gw->flow_box);

    /* Bottom bar: status left, settings gear right */
    GtkWidget *bottom = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(bottom), "bottom-bar");
    gtk_box_pack_end(GTK_BOX(root), bottom, FALSE, FALSE, 0);

    gw->status_label = gtk_label_new(NULL);
    refresh_status(gw);
    gtk_style_context_add_class(gtk_widget_get_style_context(gw->status_label),
                                "status-label");
    gtk_label_set_xalign(GTK_LABEL(gw->status_label), 0.0f);
    gtk_widget_set_hexpand(gw->status_label, TRUE);
    gtk_box_pack_start(GTK_BOX(bottom), gw->status_label, TRUE, TRUE, 0);

    /* ⚙ Settings button — right side of bottom bar */
    gw->settings_btn = gtk_button_new_from_icon_name("preferences-system-symbolic",
                                                      GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(gw->settings_btn),
                                "settings-btn");
    gtk_box_pack_end(GTK_BOX(bottom), gw->settings_btn, FALSE, FALSE, 0);
    g_signal_connect(gw->settings_btn, "clicked", G_CALLBACK(on_settings_btn), gw);

    reload_gallery(gw);
    return gw;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */
void gallery_window_show(GalleryWindow *gw)
{
    refresh_status(gw);
    gtk_widget_show_all(gw->window);
    gtk_window_present(GTK_WINDOW(gw->window));
}

void gallery_window_hide(GalleryWindow *gw)    { gtk_widget_hide(gw->window); }
gboolean gallery_window_visible(GalleryWindow *gw) { return gtk_widget_get_visible(gw->window); }
gchar *gallery_get_selected(GalleryWindow *gw)  { return gw->selected_path; }
void gallery_window_free(GalleryWindow *gw)     { if (gw) { g_free(gw->selected_path); g_free(gw); } }

void gallery_window_set_status_cb(GalleryWindow *gw,
                                   void (*cb)(gpointer), gpointer data)
{
    gw->on_status_change = cb;
    gw->on_status_data   = data;
}

void gallery_window_set_settings_cb(GalleryWindow *gw,
                                     void (*cb)(gpointer), gpointer data)
{
    gw->on_settings_open = cb;
    gw->on_settings_data = data;
}
