/**
 * @file main_gui.c
 * @brief IEC 104 Client — GTK4 Application entry point
 *
 * Loads config, creates connections, launches GTK4 window,
 * and wires connection callbacks to GUI updates via g_idle_add.
 *
 * Usage: iec104_client_gui [config.json]
 */
#include "iec104_types.h"
#include "iec104_model.h"
#include "iec104_config.h"
#include "iec104_conn.h"
#include "iec104_cmd.h"
#include "iec104_log.h"
#include "iec104_gui.h"
#include "iec104_ringbuf.h"

#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* ---- Global state (static to avoid stack overflow) ---- */
static IEC104_StationConfig s_configs[IEC104_MAX_CONNECTIONS];
static IEC104_Connection    s_connections[IEC104_MAX_CONNECTIONS];
static int                  s_num_configs = 0;
static IEC104_Gui           s_gui;
static const char          *s_config_path = NULL;

/* VULN-1/4 fix: lock-free ring buffer replaces per-point g_idle_add */
static IEC104_PointRing s_point_ring;
static int              s_auto_point_count = 0;

/* ============================================================================
 * g_idle_add callbacks — marshal connection events to GTK main thread
 * ========================================================================= */
typedef struct {
    int              conn_idx;
    IEC104_ConnState state;
} StateUpdateData;

static gboolean idle_state_update(gpointer data)
{
    StateUpdateData *d = (StateUpdateData *)data;
    iec104_gui_update_status(&s_gui, d->conn_idx, d->state);

    char buf[128];
    snprintf(buf, sizeof(buf), "[%s] %s",
             s_configs[d->conn_idx].name,
             iec104_conn_state_name(d->state));
    IEC104_LogLevel lv = (d->state == CONN_CONNECTED) ? LOG_SUCCESS :
                         (d->state == CONN_ERROR)     ? LOG_ERROR   : LOG_INFO;
    iec104_gui_log_message(&s_gui, lv, buf);

    g_free(d);
    return G_SOURCE_REMOVE;
}

void cb_state_change(const char *conn_id, IEC104_ConnState state,
                     void *user_data)
{
    (void)conn_id;
    int idx = (int)(intptr_t)user_data;
    StateUpdateData *d = g_new(StateUpdateData, 1);
    d->conn_idx = idx;
    d->state = state;
    g_idle_add(idle_state_update, d);
}

/* Auto-create a group for an IOA not covered by any existing group.
 * VULN-1 fix: capped at IEC104_MAX_AUTO_POINTS total. */
static IEC104_DataPoint* auto_create_point(IEC104_StationConfig *cfg,
                                            int ioa, int asdu_type)
{
    const char *tname = iec104_asdu_type_name((IEC104_AsduType)asdu_type);
    char grp_name[IEC104_MAX_NAME_LEN];
    snprintf(grp_name, sizeof(grp_name), "Auto_%s", tname);

    IEC104_DataGroup *grp = iec104_station_find_group(cfg, grp_name);
    if (grp) {
        IEC104_DataPoint *pt = iec104_group_get_point(grp, ioa);
        if (pt) return pt;   /* already tracked */

        /* VULN-1 cap */
        if (s_auto_point_count >= IEC104_MAX_AUTO_POINTS)
            return NULL;

        if (grp->point_count < IEC104_MAX_POINTS_PER_GRP) {
            int idx = grp->point_count;
            char pt_name[IEC104_MAX_NAME_LEN];
            snprintf(pt_name, sizeof(pt_name), "%s_%d", tname, ioa);
            iec104_point_init(&grp->points[idx], ioa, pt_name,
                              (IEC104_AsduType)asdu_type);
            grp->point_count++;
            s_auto_point_count++;
            return &grp->points[idx];
        }
        return NULL;
    }

    /* VULN-1 cap */
    if (s_auto_point_count >= IEC104_MAX_AUTO_POINTS)
        return NULL;

    if (cfg->group_count >= IEC104_MAX_GROUPS)
        return NULL;

    IEC104_DataGroup new_grp;
    iec104_group_init(&new_grp, grp_name,
                      (IEC104_AsduType)asdu_type, ioa, 1, "Auto-discovered");
    iec104_station_add_group(cfg, &new_grp);
    s_auto_point_count++;

    return iec104_station_find_point_by_type(cfg, ioa, (IEC104_AsduType)asdu_type);
}

/* ============================================================================
 * cb_point_update — protocol thread pushes to ring (zero heap alloc)
 * ========================================================================= */
void cb_point_update(const char *conn_id, int ioa,
                     IEC104_Value val, IEC104_Quality q,
                     uint64_t ts_ms, int asdu_type, void *user_data)
{
    (void)conn_id;
    IEC104_RingEntry entry;
    entry.conn_idx  = (int)(intptr_t)user_data;
    entry.ioa       = ioa;
    entry.val       = val;
    entry.q         = q;
    entry.ts_ms     = ts_ms;
    entry.asdu_type = asdu_type;
    iec104_ring_push(&s_point_ring, &entry);  /* drop if full */
    fprintf(stderr, "[RING] push conn=%d ioa=%d type=%d val_type=%d\n",
            entry.conn_idx, entry.ioa, entry.asdu_type, entry.val.type);
}

/* ---- Log callback ---- */
typedef struct {
    IEC104_LogLevel level;
    char            msg[256];
} LogData;

static gboolean idle_log(gpointer data)
{
    LogData *d = (LogData *)data;
    iec104_gui_log_message(&s_gui, d->level, d->msg);
    g_free(d);
    return G_SOURCE_REMOVE;
}

void cb_log(const char *conn_id, IEC104_LogLevel level,
            const char *msg, void *user_data)
{
    (void)user_data;
    LogData *d = g_new(LogData, 1);
    d->level = level;
    snprintf(d->msg, sizeof(d->msg), "[%s] %s", conn_id, msg);
    g_idle_add(idle_log, d);
}

/* ============================================================================
 * tick_drain_ring — 100 ms GTK timer: drains the ring buffer in batch.
 * Replaces both idle_point_update and tick_refresh.
 * ========================================================================= */
static gboolean tick_drain_ring(gpointer data)
{
    (void)data;
    IEC104_RingEntry e;
    bool any_update = false;
    bool any_new_pt = false;
    bool sld_dirty  = false;
    static int last_active = -1;

    while (iec104_ring_pop(&s_point_ring, &e)) {
        fprintf(stderr, "[DRAIN] pop conn=%d ioa=%d type=%d\n",
                e.conn_idx, e.ioa, e.asdu_type);
        if (e.conn_idx < 0 || e.conn_idx >= s_gui.num_configs)
            continue;

        IEC104_DataPoint *pt = iec104_station_find_point_by_type(
            &s_configs[e.conn_idx], e.ioa, (IEC104_AsduType)e.asdu_type);

        fprintf(stderr, "[DRAIN]   find_by_type → %s\n", pt ? "FOUND" : "NULL");

        if (!pt) {
            /* No auto-creation — only JSON-defined groups are used.
             * Unknown IOAs from the server are silently dropped. */
            fprintf(stderr, "[DRAIN]   no matching group for ioa=%d type=%d — skipped\n",
                    e.ioa, e.asdu_type);
        }
        if (pt) {
            iec104_point_update(pt, e.val, e.q, e.ts_ms);
            any_update = true;
        }

        /* ---- SLD live update: feed switch status and measurements ---- */
        {
            IEC104_AsduType t = (IEC104_AsduType)e.asdu_type;
            /* Single-point / Double-point → switch status */
            if (t == ASDU_M_SP_NA_1 || t == ASDU_M_SP_TA_1 ||
                t == ASDU_M_SP_TB_1 ||
                t == ASDU_M_DP_NA_1 || t == ASDU_M_DP_TA_1 ||
                t == ASDU_M_DP_TB_1) {
                bool closed = false;
                fprintf(stderr, "[DRAIN]   switch ioa=%d → %s\n",
                        e.ioa, closed ? "CLOSED" : "OPEN");
                if (e.val.type == VAL_BOOL)
                    closed = e.val.v.b;
                else if (e.val.type == VAL_DOUBLE_POINT || e.val.type == VAL_INT32)
                    closed = (e.val.v.i == 2);   /* DPI: 2=ON/closed */
                bool hit = sld_update_switch(&s_gui.sld, e.ioa, closed);
                if (hit) {
                    sld_dirty = true;
                    fprintf(stderr, "[SLD] switch ioa=%d → %s\n",
                            e.ioa, closed ? "CLOSED" : "OPEN");
                }
            }
            /* Measured values → telemetry (V, I, P) */
            if (t == ASDU_M_ME_NA_1 || t == ASDU_M_ME_TA_1 ||
                t == ASDU_M_ME_NB_1 || t == ASDU_M_ME_TB_1 ||
                t == ASDU_M_ME_NC_1 || t == ASDU_M_ME_TC_1 ||
                t == ASDU_M_ME_ND_1 ||
                t == ASDU_M_ME_TD_1 || t == ASDU_M_ME_TE_1 ||
                t == ASDU_M_ME_TF_1) {
                float fval = 0.0f;
                if (e.val.type == VAL_FLOAT)
                    fval = e.val.v.f;
                else if (e.val.type == VAL_INT32)
                    fval = (float)e.val.v.i;
                if (sld_update_measurement(&s_gui.sld, e.ioa, fval))
                    sld_dirty = true;
            }
        }
    }

    /* SLD redraw if any switch or measurement changed */
    if (sld_dirty) {
        sld_update_topology(&s_gui.sld);
        sld_queue_redraw(&s_gui.sld);
    }

    /* Force refresh when the user switches to a different connection
     * in the sidebar — show already-cached data immediately. */
    if (s_gui.active_conn != last_active) {
        last_active = s_gui.active_conn;
        s_gui.data_dirty = true;
    }

    if (any_update && s_gui.active_conn >= 0) {
        if (any_new_pt) {
            iec104_gui_populate_sidebar(&s_gui);
            iec104_gui_populate_data(&s_gui, s_gui.active_conn, NULL);
            s_gui.active_group[0] = '\0';
        }
        s_gui.data_dirty = true;
    }

    if (s_gui.data_dirty) {
        s_gui.data_dirty = false;
        iec104_gui_refresh_data(&s_gui, s_gui.active_conn);
    }

    return G_SOURCE_CONTINUE;
}

/* ============================================================================
 * GtkApplication activate — builds the window
 * ========================================================================= */
static void activate(GtkApplication *app, gpointer user_data)
{
    (void)user_data;

    fprintf(stderr, "[GUI] activate called\n");

    /* Load config */
    if (s_config_path) {
        fprintf(stderr, "[GUI] Loading config: %s\n", s_config_path);
        s_num_configs = iec104_config_load(s_config_path, s_configs,
                                           IEC104_MAX_CONNECTIONS);
    }

    if (s_num_configs <= 0) {
        fprintf(stderr, "[GUI] No config loaded — starting with empty state.\n");
        fprintf(stderr, "[GUI] Use '+ New' button to create connections.\n");
        s_num_configs = 0;
    }

    fprintf(stderr, "[GUI] %d connection(s) loaded\n", s_num_configs);

    /* Initialize connections */
    for (int i = 0; i < s_num_configs; i++) {
        iec104_conn_init(&s_connections[i], s_configs[i].name, &s_configs[i]);
        s_connections[i].on_state_change = cb_state_change;
        s_connections[i].on_point_update = cb_point_update;
        s_connections[i].on_log          = cb_log;
        s_connections[i].cb_user_data    = (void *)(intptr_t)i;
        s_connections[i].auto_time_sync  = true;
        s_connections[i].time_sync_interval_sec = 3600;
    }

    /* Build GUI */
    iec104_gui_init(&s_gui, app, s_configs, s_num_configs);
    s_gui.connections = s_connections;
    if (s_config_path)
        snprintf(s_gui.config_path, sizeof(s_gui.config_path), "%s", s_config_path);
    iec104_gui_build(&s_gui);

    /* Show initial data for first connection (if any) */
    if (s_num_configs > 0)
        iec104_gui_populate_data(&s_gui, 0, NULL);

    /* Initialise ring buffer and start 100 ms drain timer */
    iec104_ring_init(&s_point_ring);
    g_timeout_add(100, tick_drain_ring, NULL);

    fprintf(stderr, "[GUI] Window created OK\n");
}

/* ============================================================================
 * Main
 * ========================================================================= */
int main(int argc, char *argv[])
{
    /* Force unbuffered output for MSYS2 */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    fprintf(stderr, "[GUI] Starting IEC 104 Client GUI\n");
    fprintf(stderr, "[GUI] argc=%d\n", argc);
    for (int i = 0; i < argc; i++)
        fprintf(stderr, "[GUI] argv[%d]=%s\n", i, argv[i]);

    /* Extract our config path from argv BEFORE passing to GTK.
     * GtkApplication will interpret non-option args as files to open,
     * which triggers "open" signal instead of "activate". We strip them. */
    int gtk_argc = 1;  /* only pass argv[0] to GTK */
    char *gtk_argv[2] = { argv[0], NULL };

    for (int i = 1; i < argc; i++) {
        if (argv[i][0] != '-' && !s_config_path) {
            s_config_path = argv[i];
            fprintf(stderr, "[GUI] Config path: %s\n", s_config_path);
        }
    }

    iec104_log_init(NULL);

    /* Use FLAGS_NONE to avoid GApplication's file-opening behavior.
     * G_APPLICATION_NON_UNIQUE allows multiple instances. */
    GtkApplication *app = gtk_application_new("com.iec104.client",
                                               G_APPLICATION_NON_UNIQUE);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);

    fprintf(stderr, "[GUI] Running GtkApplication\n");
    int status = g_application_run(G_APPLICATION(app), gtk_argc, gtk_argv);
    fprintf(stderr, "[GUI] GtkApplication exited with status=%d\n", status);

    /* Cleanup */
    for (int i = 0; i < s_num_configs; i++)
        iec104_conn_destroy(&s_connections[i]);

    iec104_log_shutdown();
    g_object_unref(app);
    return status;
}
