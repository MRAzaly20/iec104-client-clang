/**
 * @file iec104_sld.c
 * @brief Single-Line Diagram — Double Bus Single Breaker Scheme
 *
 * Layout matches the IEC standard double bus single breaker diagram:
 *   Bus-II (top), Bus-I (below), V-shaped bus selectors per bay,
 *   junction point with bypass DS-D, CB, DS-C, Earth Switch, Feeder/XFMR.
 */
#include "iec104_sld.h"
#include "iec104_gui.h"
#include "iec104_cmd.h"
#include "iec104_conn.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

/* ============================================================================
 * Layout Constants
 * ========================================================================= */
#define SLD_W            1100.0
#define SLD_H            720.0

/* Busbars */
#define BUS_II_Y         55.0
#define BUS_I_Y          95.0
#define BUS_X1           40.0
#define BUS_X2           1060.0
#define BUS_THICK        5.0

/* Bay column X centres */
#define BAY1_X           160.0
#define BAY2_X           380.0
#define BAY3_X           600.0
#define BAY4_X           820.0
#define COUPLER_X        980.0

/* Vertical positions per feeder/xfmr bay */
#define DS_SPREAD        28.0    /* horizontal offset of DS-A / DS-B from cx */
#define DS_BUS_Y         140.0   /* bus selector DS center Y */
#define JUNC_Y           190.0   /* junction point (T-bar) */
#define CB_Y             255.0
#define DS_C_Y           325.0
#define ES_LINE_Y        370.0   /* ES line directly below DS-C (OHL only) */
#define ES_LINE_XOFF     30.0    /* DS-D horizontal offset from cx */
#define FEED_Y           420.0
#define MEAS_Y           490.0

/* Element sizes */
#define CB_SZ            28.0
#define DS_LN            22.0
#define XFMR_R           20.0
#define ES_SZ            14.0

/* ---- Colours ---- */
#define C_EN_R   0.18
#define C_EN_G   0.80
#define C_EN_B   0.18
#define C_DEAD_R 0.42
#define C_DEAD_G 0.42
#define C_DEAD_B 0.42
#define C_CL_R   0.10
#define C_CL_G   0.72
#define C_CL_B   0.10
#define C_OP_R   0.90
#define C_OP_G   0.12
#define C_OP_B   0.12
#define C_BG_R   0.06
#define C_BG_G   0.07
#define C_BG_B   0.10

/* ============================================================================
 * Drawing Primitives
 * ========================================================================= */

static void draw_busbar(cairo_t *cr, double x1, double x2, double y,
                         double r, double g, double b, const char *lbl)
{
    cairo_set_source_rgb(cr, r, g, b);
    cairo_set_line_width(cr, BUS_THICK);
    cairo_move_to(cr, x1, y);
    cairo_line_to(cr, x2, y);
    cairo_stroke(cr);
    cairo_set_font_size(cr, 10);
    cairo_move_to(cr, x1 - 5, y - 8);
    cairo_show_text(cr, lbl);
}

static void draw_line(cairo_t *cr, double x1, double y1,
                       double x2, double y2, bool en)
{
    cairo_set_source_rgb(cr, en ? C_EN_R : C_DEAD_R,
                              en ? C_EN_G : C_DEAD_G,
                              en ? C_EN_B : C_DEAD_B);
    cairo_set_line_width(cr, 1.8);
    cairo_move_to(cr, x1, y1);
    cairo_line_to(cr, x2, y2);
    cairo_stroke(cr);
}

static void draw_cb(cairo_t *cr, SLD_Switch *sw)
{
    if (sw->ioa == 0) return;
    double x = sw->x - CB_SZ / 2, y = sw->y - CB_SZ / 2;
    cairo_set_line_width(cr, 2.0);
    cairo_rectangle(cr, x, y, CB_SZ, CB_SZ);
    if (sw->closed) {
        cairo_set_source_rgb(cr, C_CL_R, C_CL_G, C_CL_B);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, 0.2, 0.9, 0.2);
    } else {
        cairo_set_source_rgb(cr, C_BG_R, C_BG_G, C_BG_B);
        cairo_fill_preserve(cr);
        cairo_set_source_rgb(cr, C_OP_R, C_OP_G, C_OP_B);
    }
    cairo_stroke(cr);
    if (!sw->closed) {
        cairo_move_to(cr, x + 3, y + 3);
        cairo_line_to(cr, x + CB_SZ - 3, y + CB_SZ - 3);
        cairo_move_to(cr, x + CB_SZ - 3, y + 3);
        cairo_line_to(cr, x + 3, y + CB_SZ - 3);
        cairo_stroke(cr);
    }
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_set_font_size(cr, 9);
    cairo_move_to(cr, sw->x + CB_SZ / 2 + 4, sw->y + 4);
    cairo_show_text(cr, sw->label);
    sw->w = CB_SZ; sw->h = CB_SZ;
}

static void draw_ds(cairo_t *cr, SLD_Switch *sw)
{
    if (sw->ioa == 0) return;
    cairo_set_line_width(cr, 2.2);
    double half = DS_LN / 2;
    if (sw->closed) {
        cairo_set_source_rgb(cr, C_CL_R, C_CL_G, C_CL_B);
        cairo_move_to(cr, sw->x, sw->y - half);
        cairo_line_to(cr, sw->x, sw->y + half);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgb(cr, C_OP_R, C_OP_G, C_OP_B);
        cairo_move_to(cr, sw->x, sw->y + half);
        cairo_line_to(cr, sw->x - half * 0.7, sw->y - half);
        cairo_stroke(cr);
        cairo_arc(cr, sw->x, sw->y + half, 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_arc(cr, sw->x, sw->y - half, 2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, sw->x, sw->y + half, 2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_font_size(cr, 8);
    cairo_move_to(cr, sw->x + 10, sw->y + 3);
    cairo_show_text(cr, sw->label);
    sw->w = DS_LN; sw->h = DS_LN;
}

/* Draw DS-D horizontally (bypass disconnector) */
static void draw_ds_h(cairo_t *cr, SLD_Switch *sw)
{
    if (sw->ioa == 0) return;
    cairo_set_line_width(cr, 2.2);
    double half = DS_LN / 2;
    if (sw->closed) {
        cairo_set_source_rgb(cr, C_CL_R, C_CL_G, C_CL_B);
        cairo_move_to(cr, sw->x - half, sw->y);
        cairo_line_to(cr, sw->x + half, sw->y);
        cairo_stroke(cr);
    } else {
        cairo_set_source_rgb(cr, C_OP_R, C_OP_G, C_OP_B);
        cairo_move_to(cr, sw->x - half, sw->y);
        cairo_line_to(cr, sw->x + half * 0.3, sw->y - half * 0.8);
        cairo_stroke(cr);
        cairo_arc(cr, sw->x - half, sw->y, 2.5, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
    cairo_arc(cr, sw->x - half, sw->y, 2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_arc(cr, sw->x + half, sw->y, 2, 0, 2 * M_PI);
    cairo_fill(cr);
    cairo_set_source_rgb(cr, 0.7, 0.7, 0.7);
    cairo_set_font_size(cr, 8);
    cairo_move_to(cr, sw->x - 8, sw->y - 8);
    cairo_show_text(cr, sw->label);
    sw->w = DS_LN; sw->h = DS_LN;
}

/* Earth switch symbol */
static void draw_es(cairo_t *cr, SLD_Switch *sw)
{
    if (sw->ioa == 0) return;
    double x = sw->x, y = sw->y;
    if (sw->closed)
        cairo_set_source_rgb(cr, C_CL_R, C_CL_G, C_CL_B);
    else
        cairo_set_source_rgb(cr, C_OP_R, C_OP_G, C_OP_B);
    cairo_set_line_width(cr, 2.0);
    /* Vertical stub */
    cairo_move_to(cr, x, y - ES_SZ / 2);
    cairo_line_to(cr, x, y + 2);
    cairo_stroke(cr);
    /* Ground bars */
    cairo_move_to(cr, x - ES_SZ * 0.6, y + 2);
    cairo_line_to(cr, x + ES_SZ * 0.6, y + 2);
    cairo_stroke(cr);
    cairo_move_to(cr, x - ES_SZ * 0.35, y + 6);
    cairo_line_to(cr, x + ES_SZ * 0.35, y + 6);
    cairo_stroke(cr);
    cairo_move_to(cr, x - ES_SZ * 0.15, y + 10);
    cairo_line_to(cr, x + ES_SZ * 0.15, y + 10);
    cairo_stroke(cr);
    /* Label */
    cairo_set_source_rgb(cr, 0.65, 0.65, 0.65);
    cairo_set_font_size(cr, 8);
    cairo_move_to(cr, x + 10, y + 4);
    cairo_show_text(cr, sw->label);
    sw->w = ES_SZ; sw->h = ES_SZ;
}

static void draw_xfmr(cairo_t *cr, double cx, double cy, bool en)
{
    cairo_set_source_rgb(cr, en ? C_EN_R : C_DEAD_R,
                              en ? C_EN_G : C_DEAD_G,
                              en ? C_EN_B : C_DEAD_B);
    cairo_set_line_width(cr, 2.2);
    cairo_arc(cr, cx, cy - XFMR_R * 0.5, XFMR_R, 0, 2 * M_PI);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy + XFMR_R * 0.5, XFMR_R, 0, 2 * M_PI);
    cairo_stroke(cr);
}

static void draw_meas(cairo_t *cr, SLD_Measurement *m)
{
    if (!m->label) return;
    cairo_set_font_size(cr, 9);
    cairo_set_source_rgb(cr, 0.55, 0.82, 1.0);
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %.1f %s", m->label, m->value, m->unit);
    cairo_move_to(cr, m->x, m->y);
    cairo_show_text(cr, buf);
}

static void draw_arrow(cairo_t *cr, double cx, double y, bool en)
{
    cairo_set_source_rgb(cr, en ? C_EN_R : C_DEAD_R,
                              en ? C_EN_G : C_DEAD_G,
                              en ? C_EN_B : C_DEAD_B);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, cx - 8, y - 10);
    cairo_line_to(cr, cx, y);
    cairo_line_to(cr, cx + 8, y - 10);
    cairo_stroke(cr);
}

static void draw_label(cairo_t *cr, const char *txt, double cx, double y, bool en)
{
    cairo_set_font_size(cr, 11);
    cairo_set_source_rgb(cr, en ? 0.3 : 0.6, en ? 1.0 : 0.6, en ? 0.3 : 0.6);
    cairo_text_extents_t ext;
    cairo_text_extents(cr, txt, &ext);
    cairo_move_to(cr, cx - ext.width / 2, y);
    cairo_show_text(cr, txt);
}

/* Dashed bay outline */
static void draw_bay_box(cairo_t *cr, double x1, double y1, double x2, double y2)
{
    double dash[] = {6, 4};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_set_source_rgba(cr, 0.4, 0.4, 0.5, 0.35);
    cairo_set_line_width(cr, 1.0);
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
}

/* ============================================================================
 * Draw Feeder/XFMR Bay — Straight bus selector lines
 *
 *   Bus-II ────              Bus-I  ────
 *        |                        |
 *       DS-B                     DS-A
 *        |                        |
 *        └────── junction ────────┘  (horizontal T-bar)
 *                   |
 *                  CB
 *                   |
 *                 DS-C
 *                   |
 *          ES ═══┤║ (OHL only, below DS-C)
 *                   |
 *              Feeder / XFMR
 * ========================================================================= */

static void draw_feeder_bay(cairo_t *cr, SLD_Bay *bay)
{
    double cx = bay->cx;
    bool en = bay->energized;
    double ds_b_x = cx - DS_SPREAD;
    double ds_a_x = cx + DS_SPREAD;

    /* Bay outline (dashed) */
    draw_bay_box(cr, cx - 70, BUS_I_Y + 8, cx + 70, SLD_H - 25);

    /* Bay label */
    draw_label(cr, bay->name, cx, SLD_H - 32, en);

    /* ---- Bus-II straight drop to DS-B (left) ---- */
    draw_line(cr, ds_b_x, BUS_II_Y, ds_b_x, DS_BUS_Y - DS_LN / 2, en);
    draw_ds(cr, &bay->ds_b);

    /* ---- Bus-I straight drop to DS-A (right) ---- */
    draw_line(cr, ds_a_x, BUS_I_Y, ds_a_x, DS_BUS_Y - DS_LN / 2, en);
    draw_ds(cr, &bay->ds_a);

    /* ---- Straight vertical lines down + horizontal T-bar ---- */
    bool en_b = bay->ds_b.closed;
    bool en_a = bay->ds_a.closed;
    /* DS-B straight down to junction Y */
    draw_line(cr, ds_b_x, DS_BUS_Y + DS_LN / 2, ds_b_x, JUNC_Y, en_b);
    /* DS-A straight down to junction Y */
    draw_line(cr, ds_a_x, DS_BUS_Y + DS_LN / 2, ds_a_x, JUNC_Y, en_a);
    /* Horizontal T-bar connecting left, center, right */
    bool en_junc = en_a || en_b;
    draw_line(cr, ds_b_x, JUNC_Y, ds_a_x, JUNC_Y, en_junc);

    /* Junction dot at center */
    cairo_set_source_rgb(cr, en_junc ? 1 : 0.5, en_junc ? 0.9 : 0.5, en_junc ? 0.2 : 0.5);
    cairo_arc(cr, cx, JUNC_Y, 3.5, 0, 2 * M_PI);
    cairo_fill(cr);

    /* ---- Junction to CB ---- */
    draw_line(cr, cx, JUNC_Y, cx, CB_Y - CB_SZ / 2, en_junc);
    draw_cb(cr, &bay->cb);

    /* ---- CB to DS-C ---- */
    bool en_cb = en_junc && bay->cb.closed;
    draw_line(cr, cx, CB_Y + CB_SZ / 2, cx, DS_C_Y - DS_LN / 2, en_cb);
    draw_ds(cr, &bay->ds_c);

    bool en_c = en_cb && bay->ds_c.closed;

    /* ---- OHL only: ES Line (DS-D horizontal) directly below DS-C ---- */
    if (bay->type == BAY_OHL && bay->ds_d.ioa > 0) {
        /* Short stub down from DS-C to ES Line */
        draw_line(cr, cx, DS_C_Y + DS_LN / 2, cx, ES_LINE_Y, en_c);
        /* Horizontal line to DS-D */
        draw_line(cr, cx, ES_LINE_Y, bay->ds_d.x - DS_LN / 2, ES_LINE_Y, en_c);
        draw_ds_h(cr, &bay->ds_d);
        /* Ground symbol to the right of DS-D */
        double gx = bay->ds_d.x + DS_LN / 2 + 8;
        double gy = ES_LINE_Y;
        bool esg = en_c && bay->ds_d.closed;
        cairo_set_source_rgb(cr, esg ? C_CL_R : C_DEAD_R,
                                  esg ? C_CL_G : C_DEAD_G,
                                  esg ? C_CL_B : C_DEAD_B);
        cairo_set_line_width(cr, 2.0);
        cairo_move_to(cr, bay->ds_d.x + DS_LN / 2, gy);
        cairo_line_to(cr, gx, gy);
        cairo_stroke(cr);
        /* Ground bars */
        cairo_move_to(cr, gx, gy - 7); cairo_line_to(cr, gx, gy + 7); cairo_stroke(cr);
        cairo_move_to(cr, gx+4, gy - 5); cairo_line_to(cr, gx+4, gy + 5); cairo_stroke(cr);
        cairo_move_to(cr, gx+8, gy - 3); cairo_line_to(cr, gx+8, gy + 3); cairo_stroke(cr);
        cairo_set_source_rgb(cr, 0.6, 0.6, 0.6);
        cairo_set_font_size(cr, 8);
        cairo_move_to(cr, bay->ds_d.x - 12, ES_LINE_Y - 10);
        cairo_show_text(cr, "ES");
        /* Continue line below ES to feeder */
        draw_line(cr, cx, ES_LINE_Y, cx, FEED_Y, en_c);
    } else {
        /* XFMR: straight from DS-C to feeder */
        draw_line(cr, cx, DS_C_Y + DS_LN / 2, cx, FEED_Y, en_c);
    }

    /* ---- Feeder / Transformer ---- */
    if (bay->type == BAY_XFMR) {
        draw_xfmr(cr, cx, FEED_Y + XFMR_R + 5, en_c);
        /* Arrow below transformer */
        draw_line(cr, cx, FEED_Y + XFMR_R * 2 + 20, cx, FEED_Y + XFMR_R * 2 + 35, en_c);
        draw_arrow(cr, cx, FEED_Y + XFMR_R * 2 + 35, en_c);
    } else {
        draw_arrow(cr, cx, FEED_Y + 15, en_c);
        cairo_set_source_rgb(cr, en_c ? C_EN_R : C_DEAD_R,
                                  en_c ? C_EN_G : C_DEAD_G,
                                  en_c ? C_EN_B : C_DEAD_B);
        cairo_set_font_size(cr, 9);
        cairo_text_extents_t ext;
        cairo_text_extents(cr, "Feeder", &ext);
        cairo_move_to(cr, cx - ext.width / 2, FEED_Y + 30);
        cairo_show_text(cr, "Feeder");
    }

    /* Measurements */
    for (int i = 0; i < bay->meas_count; i++)
        draw_meas(cr, &bay->meas[i]);
}

/* ============================================================================
 * Draw Bus Coupler
 *
 *   Bus-II ──── DS-B ──┐
 *                      ├── CB
 *   Bus-I  ──── DS-A ──┘
 * ========================================================================= */

static void draw_coupler_bay(cairo_t *cr, SLD_Bay *bay)
{
    double cx = bay->cx;
    double ds_b_x = cx - DS_SPREAD;
    double ds_a_x = cx + DS_SPREAD;
    bool en = bay->energized;

    draw_bay_box(cr, cx - 60, BUS_I_Y + 8, cx + 60, JUNC_Y + CB_SZ + 20);

    draw_label(cr, bay->name, cx, JUNC_Y + CB_SZ + 14, en);

    /* Bus-II to DS-B (left) */
    draw_line(cr, ds_b_x, BUS_II_Y, ds_b_x, DS_BUS_Y - DS_LN / 2, true);
    draw_ds(cr, &bay->ds_b);

    /* Bus-I to DS-A (right) */
    draw_line(cr, ds_a_x, BUS_I_Y, ds_a_x, DS_BUS_Y - DS_LN / 2, true);
    draw_ds(cr, &bay->ds_a);

    /* Straight vertical lines + horizontal T-bar to junction */
    draw_line(cr, ds_b_x, DS_BUS_Y + DS_LN / 2, ds_b_x, JUNC_Y, bay->ds_b.closed);
    draw_line(cr, ds_a_x, DS_BUS_Y + DS_LN / 2, ds_a_x, JUNC_Y, bay->ds_a.closed);
    bool en_bar = bay->ds_b.closed || bay->ds_a.closed;
    draw_line(cr, ds_b_x, JUNC_Y, ds_a_x, JUNC_Y, en_bar);

    /* CB centered on horizontal T-bar */
    bay->cb.x = cx;
    bay->cb.y = JUNC_Y;
    draw_cb(cr, &bay->cb);
}

/* ============================================================================
 * Main Draw
 * ========================================================================= */

static void sld_draw(GtkDrawingArea *area, cairo_t *cr,
                     int w, int h, gpointer data)
{
    (void)area;
    SLD_State *sld = (SLD_State *)data;
    double sx = (double)w / SLD_W;
    double sy = (double)h / SLD_H;
    double s  = (sx < sy) ? sx : sy;
    cairo_scale(cr, s, s);

    /* Background */
    cairo_set_source_rgb(cr, C_BG_R, C_BG_G, C_BG_B);
    cairo_rectangle(cr, 0, 0, SLD_W, SLD_H);
    cairo_fill(cr);

    /* Title */
    cairo_set_source_rgb(cr, 0.92, 0.92, 0.92);
    cairo_set_font_size(cr, 14);
    cairo_move_to(cr, SLD_W / 2 - 180, 22);
    cairo_show_text(cr, "DOUBLE BUSBAR SCHEME");

    /* Busbars */
    draw_busbar(cr, BUS_X1, BUS_X2, BUS_II_Y, 0.90, 0.72, 0.10, "Bus-II");
    draw_busbar(cr, BUS_X1, BUS_X2, BUS_I_Y,  0.30, 0.65, 1.00, "Bus-I");

    /* All bays */
    for (int i = 0; i < sld->bay_count; i++) {
        if (sld->bays[i].type == BAY_COUPLER)
            draw_coupler_bay(cr, &sld->bays[i]);
        else
            draw_feeder_bay(cr, &sld->bays[i]);
    }

    /* Legend */
    cairo_set_font_size(cr, 9);
    cairo_set_source_rgb(cr, 0.45, 0.45, 0.45);
    cairo_move_to(cr, 30, SLD_H - 6);
    cairo_show_text(cr,
        "Click CB/DS to control  |  Green=Closed  |  Red=Open  |  Gold=Bus-II  |  Blue=Bus-I");
}

/* ============================================================================
 * Hit Detection + Command Dialog
 * ========================================================================= */

static bool hit(SLD_Switch *sw, double mx, double my)
{
    if (sw->ioa == 0) return false;
    double hw = sw->w / 2 + 6, hh = sw->h / 2 + 6;
    return mx >= sw->x - hw && mx <= sw->x + hw &&
           my >= sw->y - hh && my <= sw->y + hh;
}

typedef struct { SLD_State *sld; SLD_Switch *sw; bool ns; } CmdCtx;

static void cmd_resp(GtkDialog *dlg, int resp, gpointer d)
{
    CmdCtx *c = (CmdCtx *)d;
    if (resp == GTK_RESPONSE_ACCEPT) {
        IEC104_Gui *gui = c->sld->gui;
        if (gui && gui->connections) {
            int ci = (gui->active_conn >= 0) ? gui->active_conn : 0;
            IEC104_Connection *conn = &gui->connections[ci];
            if (conn->state == CONN_CONNECTED) {
                int dcs = c->ns ? 2 : 1;
                IEC104_CmdResult r = iec104_cmd_double(conn, c->sw->ioa, dcs, false, 1, false);
                if (r == CMD_OK) {
                    c->sw->closed = c->ns;
                    sld_update_topology(c->sld);
                    sld_queue_redraw(c->sld);
                }
                char msg[128];
                snprintf(msg, sizeof(msg), "SLD: %s %s → %s", c->sw->label,
                         c->ns ? "CLOSE" : "OPEN", r == CMD_OK ? "OK" : "FAIL");
                iec104_gui_log_message(gui, r == CMD_OK ? LOG_SUCCESS : LOG_ERROR, msg);
            }
        }
    }
    gtk_window_destroy(GTK_WINDOW(dlg));
    g_free(c);
}

/* ============================================================================
 * Interlocking Safety Logic (per operation.md)
 *
 * Rules:
 *  1. CB is the only device that may switch under load.
 *     → All DS operations require CB to be OPEN first.
 *  2. Only one bus DS (A or B) may be closed at a time.
 *     → Closing DS-A requires DS-B open, and vice versa.
 *  3. CB may only close when a complete path exists:
 *     → One bus DS closed + DS-C closed + ES open.
 *  4. ES (earthing) may only close when fully de-energized:
 *     → CB open + DS-A open + DS-B open + DS-C open.
 *  5. ES must be open before any energizing operation.
 * ========================================================================= */

/** Check interlock rules. Returns NULL if OK, or a reason string if blocked. */
static const char* sld_check_interlock(SLD_Bay *bay, SLD_Switch *sw, bool new_state)
{
    /* Coupler bays: only check CB requires both DS */
    if (bay->type == BAY_COUPLER) {
        if (sw == &bay->cb && new_state) {
            if (!bay->ds_b.closed || !bay->ds_a.closed)
                return "CB: Both DS-B and DS-A must be closed before closing Bus Coupler CB.";
        }
        if ((sw == &bay->ds_b || sw == &bay->ds_a) && !new_state) {
            if (bay->cb.closed)
                return "DS: CB must be open before opening any disconnector.";
        }
        return NULL;
    }

    /* ---- DS-B or DS-A: bus selector disconnectors ---- */
    if (sw == &bay->ds_b || sw == &bay->ds_a) {
        /* Opening or closing a DS requires CB open */
        if (bay->cb.closed)
            return "DS: CB must be open before operating any disconnector.";
        /* Closing DS-B requires DS-A to be open (and vice versa) */
        if (new_state) {
            if (sw == &bay->ds_b && bay->ds_a.closed)
                return "DS-B: DS-A (Bus-I) must be open before closing DS-B (Bus-II).\n"
                       "Only one bus may be connected at a time.";
            if (sw == &bay->ds_a && bay->ds_b.closed)
                return "DS-A: DS-B (Bus-II) must be open before closing DS-A (Bus-I).\n"
                       "Only one bus may be connected at a time.";
        }
        return NULL;
    }

    /* ---- DS-C: line disconnector ---- */
    if (sw == &bay->ds_c) {
        if (bay->cb.closed)
            return "DS-C: CB must be open before operating the line disconnector.";
        return NULL;
    }

    /* ---- CB: circuit breaker ---- */
    if (sw == &bay->cb) {
        if (new_state) {
            /* Closing CB: need complete path */
            bool bus_ds = bay->ds_a.closed || bay->ds_b.closed;
            if (!bus_ds)
                return "CB: At least one bus disconnector (DS-A or DS-B) must be closed.";
            if (!bay->ds_c.closed)
                return "CB: DS-C (line disconnector) must be closed before closing CB.";
            /* ES must be open */
            if (bay->ds_d.ioa > 0 && bay->ds_d.closed)
                return "CB: Earth Switch (ES) must be open before closing CB.";
        }
        return NULL;
    }

    /* ---- ES (ds_d): earthing switch ---- */
    if (sw == &bay->ds_d) {
        if (new_state) {
            /* Closing ES: system must be fully de-energized */
            if (bay->cb.closed)
                return "ES: CB must be open before closing the Earth Switch.";
            if (bay->ds_a.closed)
                return "ES: DS-A must be open before closing the Earth Switch.";
            if (bay->ds_b.closed)
                return "ES: DS-B must be open before closing the Earth Switch.";
            if (bay->ds_c.closed)
                return "ES: DS-C must be open before closing the Earth Switch.";
        }
        return NULL;
    }

    return NULL;
}

static void show_cmd(SLD_State *sld, SLD_Switch *sw)
{
    bool ns = !sw->closed;
    const char *act = ns ? "CLOSE" : "OPEN";

    /* Find the bay this switch belongs to */
    SLD_Bay *bay = NULL;
    for (int i = 0; i < sld->bay_count; i++) {
        SLD_Switch *all[] = {
            &sld->bays[i].ds_b, &sld->bays[i].ds_a, &sld->bays[i].ds_d,
            &sld->bays[i].cb, &sld->bays[i].ds_c, &sld->bays[i].es
        };
        for (int j = 0; j < 6; j++)
            if (all[j] == sw) { bay = &sld->bays[i]; break; }
        if (bay) break;
    }

    /* Check interlocking rules */
    const char *reason = bay ? sld_check_interlock(bay, sw, ns) : NULL;

    if (reason) {
        /* Blocked — show interlock violation dialog */
        char title[128];
        snprintf(title, sizeof(title), "⚠ INTERLOCK — %s %s blocked", sw->label, act);

        GtkWidget *dlg = gtk_dialog_new_with_buttons(title, GTK_WINDOW(sld->gui->window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            "OK", GTK_RESPONSE_CLOSE, NULL);
        GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));

        char msg[512];
        snprintf(msg, sizeof(msg), "Operation BLOCKED by interlocking:\n\n%s\n\n"
                 "Bay: %s  |  Switch: %s  |  Action: %s",
                 reason, bay->name, sw->label, act);
        GtkWidget *lbl = gtk_label_new(msg);
        gtk_label_set_wrap(GTK_LABEL(lbl), TRUE);
        gtk_widget_set_margin_top(lbl, 16);
        gtk_widget_set_margin_bottom(lbl, 16);
        gtk_widget_set_margin_start(lbl, 16);
        gtk_widget_set_margin_end(lbl, 16);
        gtk_box_append(GTK_BOX(box), lbl);

        g_signal_connect_swapped(dlg, "response", G_CALLBACK(gtk_window_destroy), dlg);
        gtk_window_present(GTK_WINDOW(dlg));

        /* Log the interlock violation */
        char log[256];
        snprintf(log, sizeof(log), "SLD INTERLOCK: %s %s blocked — %s",
                 sw->label, act, reason);
        iec104_gui_log_message(sld->gui, LOG_WARNING, log);
        return;
    }

    /* Allowed — show confirmation dialog */
    char t[128], m[256];
    snprintf(t, sizeof(t), "%s — %s", sw->label, act);
    snprintf(m, sizeof(m), "Execute %s on %s (IOA: %d)?", act, sw->label, sw->ioa);

    GtkWidget *dlg = gtk_dialog_new_with_buttons(t, GTK_WINDOW(sld->gui->window),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Cancel", GTK_RESPONSE_CANCEL, act, GTK_RESPONSE_ACCEPT, NULL);
    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    GtkWidget *lbl = gtk_label_new(m);
    gtk_widget_set_margin_top(lbl, 16);
    gtk_widget_set_margin_bottom(lbl, 16);
    gtk_widget_set_margin_start(lbl, 16);
    gtk_widget_set_margin_end(lbl, 16);
    gtk_box_append(GTK_BOX(box), lbl);

    CmdCtx *ctx = g_new(CmdCtx, 1);
    ctx->sld = sld; ctx->sw = sw; ctx->ns = ns;
    g_signal_connect(dlg, "response", G_CALLBACK(cmd_resp), ctx);
    gtk_window_present(GTK_WINDOW(dlg));
}

static void on_click(GtkGestureClick *g, int n, double x, double y, gpointer d)
{
    (void)g; (void)n;
    SLD_State *sld = (SLD_State *)d;
    int ww = gtk_widget_get_width(sld->drawing_area);
    int hh = gtk_widget_get_height(sld->drawing_area);
    double sx = (double)ww / SLD_W, sy = (double)hh / SLD_H;
    double s = (sx < sy) ? sx : sy;
    double mx = x / s, my = y / s;

    for (int i = 0; i < sld->bay_count; i++) {
        SLD_Switch *sw[] = {
            &sld->bays[i].ds_b, &sld->bays[i].ds_a, &sld->bays[i].ds_d,
            &sld->bays[i].cb, &sld->bays[i].ds_c, &sld->bays[i].es
        };
        for (int j = 0; j < 6; j++)
            if (hit(sw[j], mx, my)) { show_cmd(sld, sw[j]); return; }
    }
}

/* ============================================================================
 * Initialisation — 4 feeder bays + 1 bus coupler
 *
 *  Bay 0  OHL-1       IOA 1001-1006   meas 2001-2003
 *  Bay 1  OHL-2       IOA 1011-1016   meas 2011-2013
 *  Bay 2  XFMR-1      IOA 1021-1026   meas 2021-2023
 *  Bay 3  XFMR-2      IOA 1031-1036   meas 2031-2033
 *  Bay 4  BUS COUPLER  IOA 1041-1043
 * ========================================================================= */

static void init_feeder(SLD_Bay *b, const char *name, SLD_BayType type,
                         double cx, int ioa, int mioa)
{
    memset(b, 0, sizeof(*b));
    b->name = name;
    b->type = type;
    b->cx   = cx;

    double dsb_x = cx - DS_SPREAD;
    double dsa_x = cx + DS_SPREAD;

    b->ds_b = (SLD_Switch){ "DS-B", ioa + 0, false, dsb_x,  DS_BUS_Y,  0, 0 };
    b->ds_a = (SLD_Switch){ "DS-A", ioa + 1, false, dsa_x,  DS_BUS_Y,  0, 0 };
    b->cb   = (SLD_Switch){ "CB",   ioa + 2, false, cx,     CB_Y,      0, 0 };
    b->ds_c = (SLD_Switch){ "DS-C", ioa + 3, false, cx,     DS_C_Y,    0, 0 };
    /* ES Line: OHL bays only; XFMR bays have no ES (ioa=0 → skip) */
    if (type == BAY_OHL)
        b->ds_d = (SLD_Switch){ "ES", ioa + 4, false, cx + ES_LINE_XOFF, ES_LINE_Y, 0, 0 };
    else
        b->ds_d = (SLD_Switch){ NULL, 0, false, 0, 0, 0, 0 };
    b->es   = (SLD_Switch){ NULL,   0,       false, 0,      0,         0, 0 };

    if (mioa > 0) {
        double my = FEED_Y + 55;
        if (type == BAY_XFMR) my = FEED_Y + XFMR_R * 2 + 50;
        b->meas[0] = (SLD_Measurement){"V","kV", mioa+0, 0, cx-22, my};
        b->meas[1] = (SLD_Measurement){"I","A",  mioa+1, 0, cx-22, my+13};
        b->meas[2] = (SLD_Measurement){"P","MW", mioa+2, 0, cx-22, my+26};
        b->meas_count = 3;
    }
}

void sld_init(SLD_State *sld, IEC104_Gui *gui)
{
    memset(sld, 0, sizeof(*sld));
    sld->gui     = gui;
    sld->bus_ii_y = BUS_II_Y;
    sld->bus_i_y  = BUS_I_Y;

    init_feeder(&sld->bays[0], "Bay-1  OHL-1",  BAY_OHL,  BAY1_X, 1001, 2001);
    init_feeder(&sld->bays[1], "Bay-2  OHL-2",  BAY_OHL,  BAY2_X, 1011, 2011);
    init_feeder(&sld->bays[2], "Bay-3  XFMR-1", BAY_XFMR, BAY3_X, 1021, 2021);
    init_feeder(&sld->bays[3], "Bay-4  XFMR-2", BAY_XFMR, BAY4_X, 1031, 2031);

    /* Bus Coupler */
    {
        SLD_Bay *bc = &sld->bays[4];
        memset(bc, 0, sizeof(*bc));
        bc->name = "Bus Coupler";
        bc->type = BAY_COUPLER;
        bc->cx   = COUPLER_X;
        double dsb_x = COUPLER_X - DS_SPREAD;
        double dsa_x = COUPLER_X + DS_SPREAD;
        bc->ds_b = (SLD_Switch){"DS-B", 1041, false, dsb_x, DS_BUS_Y, 0, 0};
        bc->ds_a = (SLD_Switch){"DS-A", 1042, false, dsa_x, DS_BUS_Y, 0, 0};
        bc->cb   = (SLD_Switch){"CB",   1043, false, COUPLER_X, JUNC_Y + 10 + CB_SZ/2, 0, 0};
    }
    sld->bay_count = 5;
}

GtkWidget* sld_build_widget(SLD_State *sld)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    sld->drawing_area = gtk_drawing_area_new();
    gtk_drawing_area_set_content_width(GTK_DRAWING_AREA(sld->drawing_area), (int)SLD_W);
    gtk_drawing_area_set_content_height(GTK_DRAWING_AREA(sld->drawing_area), (int)SLD_H);
    gtk_widget_set_vexpand(sld->drawing_area, TRUE);
    gtk_widget_set_hexpand(sld->drawing_area, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(sld->drawing_area), sld_draw, sld, NULL);
    GtkGesture *ck = gtk_gesture_click_new();
    g_signal_connect(ck, "pressed", G_CALLBACK(on_click), sld);
    gtk_widget_add_controller(sld->drawing_area, GTK_EVENT_CONTROLLER(ck));
    gtk_box_append(GTK_BOX(box), sld->drawing_area);
    return box;
}

bool sld_update_switch(SLD_State *sld, int ioa, bool closed)
{
    for (int i = 0; i < sld->bay_count; i++) {
        SLD_Switch *sw[] = {
            &sld->bays[i].ds_b, &sld->bays[i].ds_a, &sld->bays[i].ds_d,
            &sld->bays[i].cb, &sld->bays[i].ds_c, &sld->bays[i].es
        };
        for (int j = 0; j < 6; j++) {
            if (sw[j]->ioa == ioa) {
                sw[j]->closed = closed;
                sld_update_topology(sld);
                return true;
            }
        }
    }
    return false;
}

bool sld_update_measurement(SLD_State *sld, int ioa, float value)
{
    for (int i = 0; i < sld->bay_count; i++)
        for (int m = 0; m < sld->bays[i].meas_count; m++)
            if (sld->bays[i].meas[m].ioa == ioa) {
                sld->bays[i].meas[m].value = value;
                return true;
            }
    return false;
}

void sld_update_topology(SLD_State *sld)
{
    for (int i = 0; i < sld->bay_count; i++) {
        SLD_Bay *b = &sld->bays[i];
        if (b->type == BAY_COUPLER) {
            b->energized = b->ds_b.closed && b->cb.closed && b->ds_a.closed;
        } else {
            bool bus_ok = b->ds_b.closed || b->ds_a.closed;
            b->energized = bus_ok && b->cb.closed && b->ds_c.closed;
        }
    }
}

void sld_queue_redraw(SLD_State *sld)
{
    if (sld->drawing_area) gtk_widget_queue_draw(sld->drawing_area);
}
