/**
 * @file iec104_gui.c
 * @brief GTK4 GUI — Full implementation
 * Phase 3: Header bar, sidebar tree, data grid, status bar, log panel.
 * Phase 4: Command panel, Edit Slave dialog, New Connection dialog.
 * Phase 5: Save Config, Export CSV, Connect All, Disconnect All.
 */
#include "iec104_gui.h"
#include "iec104_model.h"
#include "iec104_config.h"
#include "iec104_cmd.h"
#include "iec104_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

/* ============================================================================
 * Column enums
 * ========================================================================= */
enum {
    SIDE_COL_ICON = 0, SIDE_COL_NAME, SIDE_COL_CONN_IDX, SIDE_COL_GROUP_NAME,
    SIDE_N_COLS
};
enum {
    DATA_COL_IOA = 0, DATA_COL_NAME, DATA_COL_TYPE, DATA_COL_VALUE,
    DATA_COL_QUALITY, DATA_COL_TIMESTAMP, DATA_COL_CONN_IDX,
    DATA_N_COLS
};

/* Command type names for combo box */
static const char *cmd_type_names[] = {
    "Single (SC)", "Double (DC)", "Step (RC)",
    "Norm (SE_NA)", "Scaled (SE_NB)", "Float (SE_NC)", "Bitstring (BO)",
    NULL
};

/* ============================================================================
 * CSS Loading
 * ========================================================================= */
static void load_css(void)
{
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *paths[] = { "res/style.css", "../res/style.css", "style.css", NULL };

    for (int i = 0; paths[i]; i++) {
        GFile *file = g_file_new_for_path(paths[i]);
        if (g_file_query_exists(file, NULL)) {
            gtk_css_provider_load_from_file(provider, file);
            gtk_style_context_add_provider_for_display(
                gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
                GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
            g_object_unref(file);
            return;
        }
        g_object_unref(file);
    }

    const char *fb =
        "window { background-color: #0f1117; color: #d4d8e0; }"
        "headerbar { background-color: #161822; }";
    gtk_css_provider_load_from_string(provider, fb);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(), GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
}

/* ============================================================================
 * Toolbar button callbacks
 * ========================================================================= */
static void on_connect_clicked(GtkButton *b, gpointer d)
{
    IEC104_Gui *gui = (IEC104_Gui *)d; (void)b;
    if (!gui->connections || gui->active_conn < 0) return;
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    if (c->state == CONN_DISCONNECTED || c->state == CONN_ERROR) {
        iec104_conn_start(c);
        iec104_gui_log_message(gui, LOG_INFO, "Connecting...");
    }
}

static void on_disconnect_clicked(GtkButton *b, gpointer d)
{
    IEC104_Gui *gui = (IEC104_Gui *)d; (void)b;
    if (!gui->connections || gui->active_conn < 0) return;
    iec104_conn_stop(&gui->connections[gui->active_conn]);
    iec104_gui_update_status(gui, gui->active_conn, CONN_DISCONNECTED);
    iec104_gui_log_message(gui, LOG_WARNING, "Disconnected by user.");
}

static void on_gi_clicked(GtkButton *b, gpointer d)
{
    IEC104_Gui *gui = (IEC104_Gui *)d; (void)b;
    if (!gui->connections || gui->active_conn < 0) return;
    if (iec104_conn_send_gi(&gui->connections[gui->active_conn]) == 0)
        iec104_gui_log_message(gui, LOG_COMMAND, "GI sent.");
    else
        iec104_gui_log_message(gui, LOG_ERROR, "GI failed.");
}

static void on_sync_clicked(GtkButton *b, gpointer d)
{
    IEC104_Gui *gui = (IEC104_Gui *)d; (void)b;
    if (!gui->connections || gui->active_conn < 0) return;
    if (iec104_conn_send_clock_sync(&gui->connections[gui->active_conn]) == 0)
        iec104_gui_log_message(gui, LOG_COMMAND, "Clock sync sent.");
    else
        iec104_gui_log_message(gui, LOG_ERROR, "Clock sync failed.");
}

static void on_edit_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    iec104_gui_show_edit_slave((IEC104_Gui *)d);
}

static void on_new_conn_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    iec104_gui_show_new_connection((IEC104_Gui *)d);
}

static void on_save_cfg_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    iec104_gui_save_config((IEC104_Gui *)d);
}

/* ---- Load Config file chooser ---- */
static void on_load_cfg_response(GObject *source, GAsyncResult *res, gpointer d)
{
    IEC104_Gui *gui = (IEC104_Gui *)d;
    GtkFileDialog *dlg = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_open_finish(dlg, res, NULL);
    if (!file) return;   /* user cancelled */

    char *path = g_file_get_path(file);
    g_object_unref(file);
    if (!path) return;

    iec104_gui_load_config(gui, path);
    g_free(path);
}

static void on_load_cfg_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;

    GtkFileDialog *dlg = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dlg, "Load IEC 104 Configuration");

    /* Filter for JSON files */
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "JSON files (*.json)");
    gtk_file_filter_add_pattern(filter, "*.json");
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    g_object_unref(filter);
    gtk_file_dialog_set_filters(dlg, G_LIST_MODEL(filters));
    g_object_unref(filters);

    gtk_file_dialog_open(dlg, GTK_WINDOW(gui->window), NULL,
                         on_load_cfg_response, gui);
}

static void on_export_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    iec104_gui_export_csv((IEC104_Gui *)d);
}

static void on_connect_all_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    iec104_gui_connect_all((IEC104_Gui *)d);
}

static void on_disconnect_all_clicked(GtkButton *b, gpointer d)
{
    (void)b;
    iec104_gui_disconnect_all((IEC104_Gui *)d);
}

static GtkWidget* mk_btn(const char *label, const char *cls,
                          GCallback cb, gpointer d)
{
    GtkWidget *btn = gtk_button_new_with_label(label);
    gtk_widget_add_css_class(btn, "toolbar-btn");
    if (cls) gtk_widget_add_css_class(btn, cls);
    g_signal_connect(btn, "clicked", cb, d);
    return btn;
}

/* ============================================================================
 * Build Header Bar
 * ========================================================================= */
static void build_header(IEC104_Gui *gui)
{
    gui->header = gtk_header_bar_new();
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(gui->header), TRUE);

    GtkWidget *tb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *t = gtk_label_new("IEC 104 Client");
    gtk_widget_add_css_class(t, "title");
    GtkWidget *st = gtk_label_new("SCADA Protocol Monitor");
    gtk_widget_add_css_class(st, "subtitle");
    gtk_box_append(GTK_BOX(tb), t);
    gtk_box_append(GTK_BOX(tb), st);
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(gui->header), tb);

    gui->btn_connect    = mk_btn("Connect", "toolbar-btn-connect",
                                  G_CALLBACK(on_connect_clicked), gui);
    gui->btn_disconnect = mk_btn("Disconnect", "toolbar-btn-disconnect",
                                  G_CALLBACK(on_disconnect_clicked), gui);
    gui->btn_edit       = mk_btn("Edit", NULL,
                                  G_CALLBACK(on_edit_clicked), gui);
    gui->btn_new_conn   = mk_btn("+ New", NULL,
                                  G_CALLBACK(on_new_conn_clicked), gui);

    gtk_header_bar_pack_start(GTK_HEADER_BAR(gui->header), gui->btn_connect);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(gui->header), gui->btn_disconnect);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(gui->header), gui->btn_edit);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(gui->header), gui->btn_new_conn);

    gui->btn_gi   = mk_btn("GI", NULL, G_CALLBACK(on_gi_clicked), gui);
    gui->btn_sync = mk_btn("Sync", NULL, G_CALLBACK(on_sync_clicked), gui);
    gui->btn_save_cfg = mk_btn("Save", NULL,
                               G_CALLBACK(on_save_cfg_clicked), gui);
    gui->btn_load_cfg = mk_btn("Load", NULL,
                               G_CALLBACK(on_load_cfg_clicked), gui);
    gui->btn_export   = mk_btn("Export", NULL,
                               G_CALLBACK(on_export_clicked), gui);
    gui->btn_connect_all    = mk_btn("All On", "toolbar-btn-connect",
                                      G_CALLBACK(on_connect_all_clicked), gui);
    gui->btn_disconnect_all = mk_btn("All Off", "toolbar-btn-disconnect",
                                      G_CALLBACK(on_disconnect_all_clicked), gui);

    gtk_header_bar_pack_end(GTK_HEADER_BAR(gui->header), gui->btn_disconnect_all);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(gui->header), gui->btn_connect_all);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(gui->header), gui->btn_export);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(gui->header), gui->btn_load_cfg);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(gui->header), gui->btn_save_cfg);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(gui->header), gui->btn_sync);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(gui->header), gui->btn_gi);
}

/* ============================================================================
 * Sidebar
 * ========================================================================= */
static void on_sidebar_sel(GtkTreeSelection *sel, gpointer d)
{
    IEC104_Gui *gui = (IEC104_Gui *)d;
    GtkTreeIter iter; GtkTreeModel *m;
    if (!gtk_tree_selection_get_selected(sel, &m, &iter)) return;

    int ci = -1; char *gn = NULL;
    gtk_tree_model_get(m, &iter, SIDE_COL_CONN_IDX, &ci,
                       SIDE_COL_GROUP_NAME, &gn, -1);
    if (ci >= 0) {
        gui->active_conn = ci;
        if (gn && gn[0])
            snprintf(gui->active_group, sizeof(gui->active_group), "%s", gn);
        else
            gui->active_group[0] = '\0';
        iec104_gui_populate_data(gui, ci, gn);
        if (gui->connections)
            iec104_gui_update_status(gui, ci, gui->connections[ci].state);
    }
    g_free(gn);
}

static void build_sidebar(IEC104_Gui *gui)
{
    gui->sidebar_store = gtk_tree_store_new(SIDE_N_COLS,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_STRING);
    gui->sidebar_tree = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(gui->sidebar_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gui->sidebar_tree), FALSE);

    GtkTreeViewColumn *col = gtk_tree_view_column_new();
    GtkCellRenderer *ir = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, ir, FALSE);
    gtk_tree_view_column_add_attribute(col, ir, "text", SIDE_COL_ICON);
    GtkCellRenderer *nr = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, nr, TRUE);
    gtk_tree_view_column_add_attribute(col, nr, "text", SIDE_COL_NAME);
    gtk_tree_view_append_column(GTK_TREE_VIEW(gui->sidebar_tree), col);

    GtkTreeSelection *sel = gtk_tree_view_get_selection(
        GTK_TREE_VIEW(gui->sidebar_tree));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);
    g_signal_connect(sel, "changed", G_CALLBACK(on_sidebar_sel), gui);
}

/* ============================================================================
 * Data Grid
 * ========================================================================= */
static void build_data_grid(IEC104_Gui *gui)
{
    gui->data_store = gtk_list_store_new(DATA_N_COLS,
        G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT);
    gui->data_tree = gtk_tree_view_new_with_model(
        GTK_TREE_MODEL(gui->data_store));
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(gui->data_tree), TRUE);

    const struct { const char *t; int c; int w; } cols[] = {
        {"IOA",70}, {"Name",160}, {"ASDU Type",100},
        {"Value",120}, {"Quality",80}, {"Timestamp",160},
    };
    /* fix: cols needs col index */
    for (int i = 0; i < 6; i++) {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
            cols[i].t, r, "text", i, NULL);
        gtk_tree_view_column_set_resizable(c, TRUE);
        gtk_tree_view_column_set_min_width(c, cols[i].w);
        gtk_tree_view_column_set_sort_column_id(c, i);
        gtk_tree_view_append_column(GTK_TREE_VIEW(gui->data_tree), c);
    }
}

/* ============================================================================
 * Command Control Panel — Redesigned (Phase 4+)
 *
 * Two-panel layout:
 *   Left:  Command Control (type selector, target, QOC, action buttons)
 *   Right: QOC Reference info panel
 * ========================================================================= */

/* ASDU type badge strings for each command type index */
static const char *cmd_badge_names[] = {
    "C_SC_NA_1", "C_DC_NA_1", "C_RC_NA_1",
    "C_SE_NA_1", "C_SE_NB_1", "C_SE_NC_1", "C_BO_NA_1",
};

/* ---- Helper: read QU / S/E / Time from the GUI ---- */
static int  cmd_get_qu(IEC104_Gui *gui) {
    return (int)gtk_drop_down_get_selected(GTK_DROP_DOWN(gui->cmd_qu_combo));
}
static bool cmd_get_select(IEC104_Gui *gui) {
    return gtk_drop_down_get_selected(GTK_DROP_DOWN(gui->cmd_se_combo)) == 1;
}
static bool cmd_get_time(IEC104_Gui *gui) {
    return gtk_check_button_get_active(GTK_CHECK_BUTTON(gui->cmd_time_check));
}
static int  cmd_get_ioa(IEC104_Gui *gui) {
    return atoi(gtk_editable_get_text(GTK_EDITABLE(gui->cmd_ioa_entry)));
}

/* ---- Helper: check connection is ready ---- */
static IEC104_Connection* cmd_check_conn(IEC104_Gui *gui)
{
    if (!gui->connections || gui->active_conn < 0) return NULL;
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    if (c->state != CONN_CONNECTED) {
        gtk_label_set_text(GTK_LABEL(gui->cmd_status_label), "Not connected!");
        return NULL;
    }
    return c;
}

/* ---- Helper: post result to status label + log ---- */
static void cmd_post_result(IEC104_Gui *gui, IEC104_CmdResult res,
                             const char *log_msg)
{
    gtk_label_set_text(GTK_LABEL(gui->cmd_status_label),
                       res == CMD_OK ? "OK" : "FAILED");
    iec104_gui_log_message(gui, LOG_COMMAND, log_msg);
}

/* ---- SC: Single Command action (ON=true / OFF=false) ---- */
static void on_cmd_sc_on(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_single(c, ioa, true,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "SC IOA=%d ON => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

static void on_cmd_sc_off(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_single(c, ioa, false,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "SC IOA=%d OFF => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

/* ---- DC: Double Command action (dcs: 1=ON, 2=OFF, 0=INDET, 3=INTER) ---- */
static void on_cmd_dc_on(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_double(c, ioa, 1,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "DC IOA=%d ON(1) => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

static void on_cmd_dc_off(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_double(c, ioa, 2,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "DC IOA=%d OFF(2) => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

static void on_cmd_dc_indet(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_double(c, ioa, 0,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "DC IOA=%d INDET(0) => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

static void on_cmd_dc_inter(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_double(c, ioa, 3,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "DC IOA=%d INTER(3) => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

/* ---- RC: Step Command action (1=UP, 2=DOWN) ---- */
static void on_cmd_rc_up(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_step(c, ioa, 1,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "RC IOA=%d UP(1) => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

static void on_cmd_rc_down(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    IEC104_CmdResult res = iec104_cmd_step(c, ioa, 2,
        cmd_get_select(gui), cmd_get_qu(gui), cmd_get_time(gui));
    char log[128];
    snprintf(log, sizeof(log), "RC IOA=%d DOWN(2) => %s", ioa,
             res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

/* ---- SE: Setpoint Command action (reads value entry) ---- */
static void on_cmd_se_send(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    int qu = cmd_get_qu(gui);
    bool sel = cmd_get_select(gui);
    bool wt = cmd_get_time(gui);
    const char *val_str = gtk_editable_get_text(GTK_EDITABLE(gui->cmd_value_entry));
    int type_idx = gui->cmd_current_type;
    IEC104_CmdResult res = CMD_ERROR;
    char log[128];

    switch (type_idx) {
    case 3: { /* Normalized */
        float v = (float)atof(val_str);
        res = iec104_cmd_setpoint_normalized(c, ioa, v, sel, qu, wt);
        snprintf(log, sizeof(log), "SE_NA IOA=%d val=%.4f => %s",
                 ioa, v, res == CMD_OK ? "OK" : "FAIL");
        break;
    }
    case 4: { /* Scaled */
        int v = atoi(val_str);
        res = iec104_cmd_setpoint_scaled(c, ioa, v, sel, qu, wt);
        snprintf(log, sizeof(log), "SE_NB IOA=%d val=%d => %s",
                 ioa, v, res == CMD_OK ? "OK" : "FAIL");
        break;
    }
    case 5: { /* Float */
        float v = (float)atof(val_str);
        res = iec104_cmd_setpoint_float(c, ioa, v, sel, qu, wt);
        snprintf(log, sizeof(log), "SE_NC IOA=%d val=%.4f => %s",
                 ioa, v, res == CMD_OK ? "OK" : "FAIL");
        break;
    }
    default:
        snprintf(log, sizeof(log), "SE: invalid type %d", type_idx);
        break;
    }
    cmd_post_result(gui, res, log);
}

/* ---- BO: Bitstring Command action (reads hex value entry) ---- */
static void on_cmd_bo_send(GtkButton *b, gpointer d)
{
    (void)b; IEC104_Gui *gui = (IEC104_Gui *)d;
    IEC104_Connection *c = cmd_check_conn(gui); if (!c) return;
    int ioa = cmd_get_ioa(gui);
    const char *val_str = gtk_editable_get_text(GTK_EDITABLE(gui->cmd_value_entry));
    uint32_t bits = (uint32_t)strtoul(val_str, NULL, 0);
    bool wt = cmd_get_time(gui);
    IEC104_CmdResult res = iec104_cmd_bitstring(c, ioa, bits, wt);
    char log[128];
    snprintf(log, sizeof(log), "BO IOA=%d val=0x%08X => %s",
             ioa, bits, res == CMD_OK ? "OK" : "FAIL");
    cmd_post_result(gui, res, log);
}

/* ---- Clear and rebuild action buttons for the selected type ---- */
static void cmd_rebuild_actions(IEC104_Gui *gui)
{
    /* Remove all children from action box */
    GtkWidget *child = gtk_widget_get_first_child(gui->cmd_action_box);
    while (child) {
        GtkWidget *next = gtk_widget_get_next_sibling(child);
        gtk_box_remove(GTK_BOX(gui->cmd_action_box), child);
        child = next;
    }

    int t = gui->cmd_current_type;

    switch (t) {
    case 0: { /* SC: ON / OFF */
        GtkWidget *on  = mk_btn("\xe2\x86\x91 ON",  "cmd-btn-on",
                                 G_CALLBACK(on_cmd_sc_on),  gui);
        GtkWidget *off = mk_btn("\xe2\x86\x93 OFF", "cmd-btn-off",
                                 G_CALLBACK(on_cmd_sc_off), gui);
        gtk_widget_set_hexpand(on, TRUE);
        gtk_widget_set_hexpand(off, TRUE);
        gtk_box_append(GTK_BOX(gui->cmd_action_box), on);
        gtk_box_append(GTK_BOX(gui->cmd_action_box), off);
        gtk_widget_set_visible(gui->cmd_value_row, FALSE);
        break;
    }
    case 1: { /* DC: ON / OFF / INDETERMINATE / INTERMEDIATE */
        GtkWidget *on    = mk_btn("\xe2\x86\x91 ON",  "cmd-btn-on",
                                   G_CALLBACK(on_cmd_dc_on),  gui);
        GtkWidget *off   = mk_btn("\xe2\x86\x93 OFF", "cmd-btn-off",
                                   G_CALLBACK(on_cmd_dc_off), gui);
        GtkWidget *indet = mk_btn("\xe2\x97\x86 INDETERMINATE", "cmd-btn-indet",
                                   G_CALLBACK(on_cmd_dc_indet), gui);
        GtkWidget *inter = mk_btn("\xe2\x97\x87 INTERMEDIATE",  "cmd-btn-inter",
                                   G_CALLBACK(on_cmd_dc_inter), gui);
        gtk_widget_set_hexpand(on, TRUE);
        gtk_widget_set_hexpand(off, TRUE);
        gtk_widget_set_hexpand(indet, TRUE);
        gtk_widget_set_hexpand(inter, TRUE);
        GtkWidget *r1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        GtkWidget *r2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_box_append(GTK_BOX(r1), on);
        gtk_box_append(GTK_BOX(r1), off);
        gtk_box_append(GTK_BOX(r2), indet);
        gtk_box_append(GTK_BOX(r2), inter);
        GtkWidget *rows = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
        gtk_box_append(GTK_BOX(rows), r1);
        gtk_box_append(GTK_BOX(rows), r2);
        gtk_widget_set_hexpand(rows, TRUE);
        gtk_box_append(GTK_BOX(gui->cmd_action_box), rows);
        gtk_widget_set_visible(gui->cmd_value_row, FALSE);
        break;
    }
    case 2: { /* RC: UP / DOWN */
        GtkWidget *up   = mk_btn("\xe2\x96\xb2 UP",   "cmd-btn-send",
                                  G_CALLBACK(on_cmd_rc_up),   gui);
        GtkWidget *down = mk_btn("\xe2\x96\xbc DOWN", "cmd-btn-off",
                                  G_CALLBACK(on_cmd_rc_down), gui);
        gtk_widget_set_hexpand(up, TRUE);
        gtk_widget_set_hexpand(down, TRUE);
        gtk_box_append(GTK_BOX(gui->cmd_action_box), up);
        gtk_box_append(GTK_BOX(gui->cmd_action_box), down);
        gtk_widget_set_visible(gui->cmd_value_row, FALSE);
        break;
    }
    case 3: /* SE_NA */
    case 4: /* SE_NB */
    case 5: { /* SE_NC */
        GtkWidget *send = mk_btn("\xe2\x9a\xa1 Send", "cmd-btn-send",
                                  G_CALLBACK(on_cmd_se_send), gui);
        gtk_widget_set_hexpand(send, TRUE);
        gtk_box_append(GTK_BOX(gui->cmd_action_box), send);
        gtk_widget_set_visible(gui->cmd_value_row, TRUE);
        /* Range hint */
        const char *hint = (t == 3) ? "Range: -1.0 to +1.0 (normalized)"
                         : (t == 4) ? "Range: -32768 to +32767 (scaled)"
                                    : "Range: float (short floating point)";
        gtk_label_set_text(GTK_LABEL(gui->cmd_range_label), hint);
        gtk_editable_set_text(GTK_EDITABLE(gui->cmd_value_entry), "0.0");
        break;
    }
    case 6: { /* BO */
        GtkWidget *send = mk_btn("\xe2\x9a\xa1 Send", "cmd-btn-send",
                                  G_CALLBACK(on_cmd_bo_send), gui);
        gtk_widget_set_hexpand(send, TRUE);
        gtk_box_append(GTK_BOX(gui->cmd_action_box), send);
        gtk_widget_set_visible(gui->cmd_value_row, TRUE);
        gtk_label_set_text(GTK_LABEL(gui->cmd_range_label),
                           "Value: 32-bit hex (0x00000000)");
        gtk_editable_set_text(GTK_EDITABLE(gui->cmd_value_entry), "0x00000000");
        break;
    }
    }

    /* Update badge */
    if (t >= 0 && t < 7)
        gtk_label_set_text(GTK_LABEL(gui->cmd_type_badge), cmd_badge_names[t]);
}

/* ---- Type combo changed callback ---- */
static void on_cmd_type_changed(GObject *obj, GParamSpec *ps, gpointer d)
{
    (void)obj; (void)ps;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    gui->cmd_current_type = (int)gtk_drop_down_get_selected(
                                GTK_DROP_DOWN(gui->cmd_type_combo));
    cmd_rebuild_actions(gui);
}

/* ---- Data grid row click → populate command panel target ---- */
static void on_data_row_activated(GtkTreeView *tv, GtkTreePath *path,
                                  GtkTreeViewColumn *col, gpointer d)
{
    (void)col;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    GtkTreeModel *model = gtk_tree_view_get_model(tv);
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter(model, &iter, path)) return;

    int ioa = 0;
    char *name = NULL;
    char *type_str = NULL;
    gtk_tree_model_get(model, &iter,
                       DATA_COL_IOA, &ioa,
                       DATA_COL_NAME, &name,
                       DATA_COL_TYPE, &type_str, -1);

    /* Update IOA entry */
    char ioa_str[16];
    snprintf(ioa_str, sizeof(ioa_str), "%d", ioa);
    gtk_editable_set_text(GTK_EDITABLE(gui->cmd_ioa_entry), ioa_str);

    /* Update target info label */
    char tgt[128];
    snprintf(tgt, sizeof(tgt), "IOA:  %d   %s", ioa, name ? name : "");
    gtk_label_set_text(GTK_LABEL(gui->cmd_ioa_label), tgt);

    /* Store in state */
    gui->cmd_selected_ioa = ioa;
    if (name)
        snprintf(gui->cmd_selected_name, sizeof(gui->cmd_selected_name),
                 "%s", name);

    /* Try to auto-detect command type from ASDU type string */
    if (type_str) {
        const struct { const char *prefix; int idx; } map[] = {
            {"C_SC", 0}, {"C_DC", 1}, {"C_RC", 2},
            {"C_SE_NA", 3}, {"C_SE_NB", 4}, {"C_SE_NC", 5},
            {"C_SE_TA", 3}, {"C_SE_TB", 4}, {"C_SE_TC", 5},
            {"C_BO", 6}, {NULL, -1}
        };
        for (int i = 0; map[i].prefix; i++) {
            if (strncmp(type_str, map[i].prefix, strlen(map[i].prefix)) == 0) {
                gtk_drop_down_set_selected(GTK_DROP_DOWN(gui->cmd_type_combo),
                                           (guint)map[i].idx);
                break;
            }
        }
    }

    g_free(name);
    g_free(type_str);
}

/* ============================================================================
 * build_cmd_panel — Two-panel Command Control layout
 * ========================================================================= */
static void build_cmd_panel(IEC104_Gui *gui)
{
    gui->cmd_current_type = 0;
    gui->cmd_selected_ioa = 0;
    gui->cmd_selected_name[0] = '\0';

    /* ================================================================
     * Command Control Panel (standalone)
     * ================================================================ */
    gui->cmd_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(gui->cmd_frame, "cmd-frame");
    gtk_widget_set_size_request(gui->cmd_frame, 420, -1);
    gtk_widget_set_margin_start(gui->cmd_frame, 8);
    gtk_widget_set_margin_end(gui->cmd_frame, 4);
    gtk_widget_set_margin_top(gui->cmd_frame, 8);
    gtk_widget_set_margin_bottom(gui->cmd_frame, 8);

    /* Header row: title + type badge */
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *title_lbl = gtk_label_new("\xe2\x9a\xa1 Command Control");
    gtk_widget_add_css_class(title_lbl, "cmd-title");
    gtk_widget_set_halign(title_lbl, GTK_ALIGN_START);
    gtk_widget_set_hexpand(title_lbl, TRUE);
    gtk_box_append(GTK_BOX(hdr), title_lbl);

    gui->cmd_type_badge = gtk_label_new("C_SC_NA_1");
    gtk_widget_add_css_class(gui->cmd_type_badge, "cmd-badge");
    gtk_box_append(GTK_BOX(hdr), gui->cmd_type_badge);
    gtk_box_append(GTK_BOX(gui->cmd_frame), hdr);

    /* TARGET info box */
    gui->cmd_target_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
    gtk_widget_add_css_class(gui->cmd_target_frame, "cmd-target");
    gtk_widget_set_margin_top(gui->cmd_target_frame, 2);

    GtkWidget *tgt_hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *tgt_lbl = gtk_label_new("TARGET");
    gtk_widget_add_css_class(tgt_lbl, "cmd-target-label");
    gtk_widget_set_halign(tgt_lbl, GTK_ALIGN_START);
    gtk_widget_set_hexpand(tgt_lbl, TRUE);
    gtk_box_append(GTK_BOX(tgt_hdr), tgt_lbl);

    gui->cmd_status_dot = gtk_label_new("\xe2\x97\x8f Ready");
    gtk_widget_add_css_class(gui->cmd_status_dot, "cmd-status-ready");
    gtk_box_append(GTK_BOX(tgt_hdr), gui->cmd_status_dot);
    gtk_box_append(GTK_BOX(gui->cmd_target_frame), tgt_hdr);

    /* IOA display + manual entry */
    GtkWidget *ioa_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gui->cmd_ioa_label = gtk_label_new("IOA:  --");
    gtk_widget_add_css_class(gui->cmd_ioa_label, "cmd-ioa");
    gtk_widget_set_halign(gui->cmd_ioa_label, GTK_ALIGN_START);
    gtk_widget_set_hexpand(gui->cmd_ioa_label, TRUE);
    gtk_box_append(GTK_BOX(ioa_row), gui->cmd_ioa_label);

    gui->cmd_ioa_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gui->cmd_ioa_entry), "1");
    gtk_entry_set_max_length(GTK_ENTRY(gui->cmd_ioa_entry), 8);
    gtk_widget_set_size_request(gui->cmd_ioa_entry, 70, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui->cmd_ioa_entry), "IOA");
    gtk_box_append(GTK_BOX(ioa_row), gui->cmd_ioa_entry);

    gtk_box_append(GTK_BOX(gui->cmd_target_frame), ioa_row);
    gtk_box_append(GTK_BOX(gui->cmd_frame), gui->cmd_target_frame);

    /* Command type dropdown */
    GtkWidget *type_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *type_lbl = gtk_label_new("Type:");
    gtk_widget_set_size_request(type_lbl, 65, -1);
    gtk_widget_set_halign(type_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(type_row), type_lbl);

    GtkStringList *types = gtk_string_list_new(cmd_type_names);
    gui->cmd_type_combo = gtk_drop_down_new(G_LIST_MODEL(types), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(gui->cmd_type_combo), 0);
    gtk_widget_set_hexpand(gui->cmd_type_combo, TRUE);
    gtk_box_append(GTK_BOX(type_row), gui->cmd_type_combo);
    gtk_box_append(GTK_BOX(gui->cmd_frame), type_row);

    /* Qualifier row */
    GtkWidget *qu_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *qu_lbl = gtk_label_new("Qualifier:");
    gtk_widget_set_size_request(qu_lbl, 65, -1);
    gtk_widget_set_halign(qu_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(qu_row), qu_lbl);

    const char *qu_names[] = { "No Additional", "Short Pulse",
                               "Long Pulse", "Persistent", NULL };
    GtkStringList *qu_list = gtk_string_list_new(qu_names);
    gui->cmd_qu_combo = gtk_drop_down_new(G_LIST_MODEL(qu_list), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(gui->cmd_qu_combo), 2); /* Long Pulse */
    gtk_widget_set_hexpand(gui->cmd_qu_combo, TRUE);
    gtk_box_append(GTK_BOX(qu_row), gui->cmd_qu_combo);
    gtk_box_append(GTK_BOX(gui->cmd_frame), qu_row);

    /* S/E + Time row */
    GtkWidget *se_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    const char *se_names[] = { "(0) Execute", "(1) Select", NULL };
    GtkStringList *se_list = gtk_string_list_new(se_names);
    gui->cmd_se_combo = gtk_drop_down_new(G_LIST_MODEL(se_list), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(gui->cmd_se_combo), 0);
    gtk_widget_set_hexpand(gui->cmd_se_combo, TRUE);
    gtk_box_append(GTK_BOX(se_row), gui->cmd_se_combo);

    gui->cmd_time_check = gtk_check_button_new_with_label("With Time Tag");
    gtk_box_append(GTK_BOX(se_row), gui->cmd_time_check);
    gtk_box_append(GTK_BOX(gui->cmd_frame), se_row);

    /* Value row (hidden for SC/DC/RC, shown for SE/BO) */
    gui->cmd_value_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *val_lbl = gtk_label_new("Value:");
    gtk_widget_set_size_request(val_lbl, 65, -1);
    gtk_widget_set_halign(val_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(gui->cmd_value_row), val_lbl);

    gui->cmd_value_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gui->cmd_value_entry), "0.0");
    gtk_entry_set_max_length(GTK_ENTRY(gui->cmd_value_entry), 16);
    gtk_widget_set_hexpand(gui->cmd_value_entry, TRUE);
    gtk_box_append(GTK_BOX(gui->cmd_value_row), gui->cmd_value_entry);

    gtk_box_append(GTK_BOX(gui->cmd_frame), gui->cmd_value_row);
    gtk_widget_set_visible(gui->cmd_value_row, FALSE);

    /* Range hint */
    gui->cmd_range_label = gtk_label_new("");
    gtk_widget_add_css_class(gui->cmd_range_label, "cmd-range-hint");
    gtk_widget_set_halign(gui->cmd_range_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(gui->cmd_frame), gui->cmd_range_label);

    /* Action buttons container */
    gui->cmd_action_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_margin_top(gui->cmd_action_box, 4);
    gtk_box_append(GTK_BOX(gui->cmd_frame), gui->cmd_action_box);

    /* Status label */
    gui->cmd_status_label = gtk_label_new("");
    gtk_widget_add_css_class(gui->cmd_status_label, "cmd-status-result");
    gtk_widget_set_halign(gui->cmd_status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(gui->cmd_frame), gui->cmd_status_label);

    /* Connect type-change signal */
    g_signal_connect(gui->cmd_type_combo, "notify::selected",
                     G_CALLBACK(on_cmd_type_changed), gui);

    /* Build initial action buttons for SC type */
    cmd_rebuild_actions(gui);
}

/* ============================================================================
 * build_qoc_panel — Qualifier of Command Reference (standalone panel)
 * ========================================================================= */
static void build_qoc_panel(IEC104_Gui *gui)
{
    gui->cmd_qoc_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_add_css_class(gui->cmd_qoc_frame, "cmd-qoc");
    gtk_widget_set_margin_start(gui->cmd_qoc_frame, 4);
    gtk_widget_set_margin_end(gui->cmd_qoc_frame, 4);
    gtk_widget_set_margin_top(gui->cmd_qoc_frame, 4);
    gtk_widget_set_margin_bottom(gui->cmd_qoc_frame, 4);

    GtkWidget *qoc_title = gtk_label_new("\xf0\x9f\x93\x8b Qualifier of Command (QOC)");
    gtk_widget_add_css_class(qoc_title, "cmd-title");
    gtk_widget_set_halign(qoc_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(gui->cmd_qoc_frame), qoc_title);

    const char *qoc_lines[] = {
        "QU            := UI5[3..7](0..31)",
        "  (0)         := no additional definition",
        "  (1)         := short pulse duration",
        "  (2)         := long pulse duration",
        "  (3)         := persistent output",
        "",
        "S/E           := BS1[8](0..1)",
        "  (0) execute := (1) select",
        NULL
    };

    for (int i = 0; qoc_lines[i]; i++) {
        GtkWidget *ln = gtk_label_new(qoc_lines[i]);
        gtk_widget_add_css_class(ln, "cmd-qoc-line");
        gtk_widget_set_halign(ln, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(gui->cmd_qoc_frame), ln);
    }
}

/* ============================================================================
 * System Commands Panel (Phase 6)
 * ========================================================================= */
static void on_sys_gi(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    if (gui->active_conn < 0 || !gui->connections) return;
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    int ret = iec104_conn_send_gi(c);
    iec104_gui_log_message(gui, ret == 0 ? LOG_COMMAND : LOG_ERROR,
                           ret == 0 ? "GI (C_IC_NA_1) sent" : "GI send failed");
}

static void on_sys_counter(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    if (gui->active_conn < 0 || !gui->connections) return;
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    int ret = iec104_conn_send_counter_interrogation(c);
    iec104_gui_log_message(gui, ret == 0 ? LOG_COMMAND : LOG_ERROR,
                           ret == 0 ? "Counter Interr. (C_CI_NA_1) sent"
                                    : "Counter Interr. send failed");
}

static void on_sys_sync(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    if (gui->active_conn < 0 || !gui->connections) return;
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    int ret = iec104_conn_send_clock_sync(c);
    iec104_gui_log_message(gui, ret == 0 ? LOG_COMMAND : LOG_ERROR,
                           ret == 0 ? "Clock Sync (C_CS_NA_1) sent"
                                    : "Clock Sync send failed");
}

static void on_sys_test(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    if (gui->active_conn < 0 || !gui->connections) return;
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    int ret = iec104_conn_send_test(c);
    iec104_gui_log_message(gui, ret == 0 ? LOG_COMMAND : LOG_ERROR,
                           ret == 0 ? "Test (C_TS_NA_1) sent"
                                    : "Test send failed");
}

static void on_sys_reset(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    if (gui->active_conn < 0 || !gui->connections) return;
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    int ret = iec104_conn_send_reset_process(c, 1); /* QRP=1 general reset */
    iec104_gui_log_message(gui, ret == 0 ? LOG_COMMAND : LOG_ERROR,
                           ret == 0 ? "Reset Process (C_RP_NA_1) sent"
                                    : "Reset Process send failed");
}

static void on_sys_read(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    if (gui->active_conn < 0 || !gui->connections) return;
    int ioa = atoi(gtk_editable_get_text(GTK_EDITABLE(gui->sys_read_ioa)));
    if (ioa <= 0) {
        iec104_gui_log_message(gui, LOG_WARNING, "Read: invalid IOA");
        return;
    }
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    int ret = iec104_conn_send_read(c, ioa);
    char msg[128];
    snprintf(msg, sizeof(msg), ret == 0
             ? "Read (C_RD_NA_1) IOA=%d sent" : "Read IOA=%d failed", ioa);
    iec104_gui_log_message(gui, ret == 0 ? LOG_COMMAND : LOG_ERROR, msg);
}

static void on_sys_delay(GtkButton *b, gpointer d)
{
    (void)b;
    IEC104_Gui *gui = (IEC104_Gui *)d;
    if (gui->active_conn < 0 || !gui->connections) return;
    int ms = atoi(gtk_editable_get_text(GTK_EDITABLE(gui->sys_delay_entry)));
    if (ms <= 0) {
        iec104_gui_log_message(gui, LOG_WARNING, "Delay: invalid value");
        return;
    }
    IEC104_Connection *c = &gui->connections[gui->active_conn];
    int ret = iec104_conn_send_delay_acquisition(c, (uint16_t)ms);
    char msg[128];
    snprintf(msg, sizeof(msg), ret == 0
             ? "Delay Acq. (C_CD_NA_1) %d ms sent"
             : "Delay Acq. %d ms failed", ms);
    iec104_gui_log_message(gui, ret == 0 ? LOG_COMMAND : LOG_ERROR, msg);
}

static void build_sys_panel(IEC104_Gui *gui)
{
    /* ================================================================
     * LEFT CARD — Interrogation & Sync
     * ================================================================ */
    gui->sys_frame = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(gui->sys_frame, "cmd-frame");
    gtk_widget_set_size_request(gui->sys_frame, 320, -1);
    gtk_widget_set_margin_start(gui->sys_frame, 4);
    gtk_widget_set_margin_end(gui->sys_frame, 4);
    gtk_widget_set_margin_top(gui->sys_frame, 4);
    gtk_widget_set_margin_bottom(gui->sys_frame, 4);

    GtkWidget *sys_l_title = gtk_label_new("\xf0\x9f\x94\x8d Interrogation & Sync");
    gtk_widget_add_css_class(sys_l_title, "cmd-title");
    gtk_widget_set_halign(sys_l_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(gui->sys_frame), sys_l_title);

    /* GI + Counter row */
    GtkWidget *r1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *gi_btn = mk_btn("\xe2\x9a\xa1 GI (100)", "cmd-btn-send",
                                G_CALLBACK(on_sys_gi), gui);
    /* natural button width */
    gtk_box_append(GTK_BOX(r1), gi_btn);
    GtkWidget *ci_btn = mk_btn("\xf0\x9f\x94\xa2 Counter (101)", "cmd-btn-send",
                                G_CALLBACK(on_sys_counter), gui);
    /* natural button width */
    gtk_box_append(GTK_BOX(r1), ci_btn);
    gtk_box_append(GTK_BOX(gui->sys_frame), r1);

    /* Sync + Test row */
    GtkWidget *r2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *sync_btn = mk_btn("\xe2\x8f\xb1 Sync Clock (103)", "cmd-btn-on",
                                  G_CALLBACK(on_sys_sync), gui);
    /* natural button width */
    gtk_box_append(GTK_BOX(r2), sync_btn);
    GtkWidget *test_btn = mk_btn("\xf0\x9f\x94\xa7 Test (104)", "cmd-btn-indet",
                                  G_CALLBACK(on_sys_test), gui);
    /* natural button width */
    gtk_box_append(GTK_BOX(r2), test_btn);
    gtk_box_append(GTK_BOX(gui->sys_frame), r2);

    /* Reset row */
    GtkWidget *reset_btn = mk_btn("\xe2\x9a\xa0 Reset Process (105)", "cmd-btn-off",
                                   G_CALLBACK(on_sys_reset), gui);
    gtk_box_append(GTK_BOX(gui->sys_frame), reset_btn);

    /* ================================================================
     * RIGHT CARD — Point Read & Delay
     * ================================================================ */
    gui->sys_delay_entry = NULL; /* init */
    GtkWidget *sys_right = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_add_css_class(sys_right, "cmd-frame");
    gtk_widget_set_size_request(sys_right, 320, -1);
    gtk_widget_set_margin_start(sys_right, 4);
    gtk_widget_set_margin_end(sys_right, 4);
    gtk_widget_set_margin_top(sys_right, 4);
    gtk_widget_set_margin_bottom(sys_right, 4);

    GtkWidget *sys_r_title = gtk_label_new("\xf0\x9f\x93\x96 Point Read & Delay");
    gtk_widget_add_css_class(sys_r_title, "cmd-title");
    gtk_widget_set_halign(sys_r_title, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sys_right), sys_r_title);

    /* Read IOA row */
    GtkWidget *rd_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *rd_lbl = gtk_label_new("IOA:");
    gtk_widget_set_size_request(rd_lbl, 50, -1);
    gtk_widget_set_halign(rd_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(rd_row), rd_lbl);

    gui->sys_read_ioa = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gui->sys_read_ioa), "1");
    gtk_entry_set_max_length(GTK_ENTRY(gui->sys_read_ioa), 8);
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui->sys_read_ioa), "IOA");
    gtk_widget_set_hexpand(gui->sys_read_ioa, TRUE);
    gtk_box_append(GTK_BOX(rd_row), gui->sys_read_ioa);

    GtkWidget *rd_btn = mk_btn("\xf0\x9f\x93\xa5 Read (102)", "cmd-btn-send",
                                G_CALLBACK(on_sys_read), gui);
    /* natural button width */
    gtk_box_append(GTK_BOX(rd_row), rd_btn);
    gtk_box_append(GTK_BOX(sys_right), rd_row);

    /* Delay Acquisition row */
    GtkWidget *dl_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *dl_lbl = gtk_label_new("Delay:");
    gtk_widget_set_size_request(dl_lbl, 50, -1);
    gtk_widget_set_halign(dl_lbl, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(dl_row), dl_lbl);

    gui->sys_delay_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(gui->sys_delay_entry), "1000");
    gtk_entry_set_max_length(GTK_ENTRY(gui->sys_delay_entry), 8);
    gtk_entry_set_placeholder_text(GTK_ENTRY(gui->sys_delay_entry), "ms");
    gtk_widget_set_hexpand(gui->sys_delay_entry, TRUE);
    gtk_box_append(GTK_BOX(dl_row), gui->sys_delay_entry);

    GtkWidget *dl_btn = mk_btn("\xe2\x8f\xb3 Delay Acq (106)", "cmd-btn-send",
                                G_CALLBACK(on_sys_delay), gui);
    /* natural button width */
    gtk_box_append(GTK_BOX(dl_row), dl_btn);
    gtk_box_append(GTK_BOX(sys_right), dl_row);

    /* Stash right card pointer on sys_frame as data so gui_build can retrieve it */
    g_object_set_data(G_OBJECT(gui->sys_frame), "sys-right", sys_right);
}

/* coalesce_tick removed — tick_drain_ring in main_gui.c now handles
 * both ring-buffer draining and data_dirty refresh in a single timer. */


/* ============================================================================
 * Log panel
 * ========================================================================= */
static void build_log_panel(IEC104_Gui *gui)
{
    gui->log_buffer = gtk_text_buffer_new(NULL);
    gui->log_view = gtk_text_view_new_with_buffer(gui->log_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gui->log_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(gui->log_view), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(gui->log_view), FALSE);
    gtk_widget_add_css_class(gui->log_view, "log-view");

    gtk_text_buffer_create_tag(gui->log_buffer, "info",
        "foreground", "#60b0f4", NULL);
    gtk_text_buffer_create_tag(gui->log_buffer, "success",
        "foreground", "#4ae68a", NULL);
    gtk_text_buffer_create_tag(gui->log_buffer, "warning",
        "foreground", "#e6c84a", NULL);
    gtk_text_buffer_create_tag(gui->log_buffer, "error",
        "foreground", "#e64a4a", "weight", PANGO_WEIGHT_BOLD, NULL);
    gtk_text_buffer_create_tag(gui->log_buffer, "command",
        "foreground", "#c084fc", NULL);
}

/* ============================================================================
 * Status bar
 * ========================================================================= */
static gboolean tick_clock(gpointer d)
{
    IEC104_Gui *gui = (IEC104_Gui *)d;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char buf[16]; strftime(buf, sizeof(buf), "%H:%M:%S", t);
    gtk_label_set_text(GTK_LABEL(gui->time_label), buf);
    return G_SOURCE_CONTINUE;
}

static void build_status_bar(IEC104_Gui *gui)
{
    gui->status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_add_css_class(gui->status_bar, "status-bar");

    gui->status_pill = gtk_label_new("DISCONNECTED");
    gtk_widget_add_css_class(gui->status_pill, "status-disconnected");
    gtk_box_append(GTK_BOX(gui->status_bar), gui->status_pill);

    gui->status_label = gtk_label_new("No connection");
    gtk_box_append(GTK_BOX(gui->status_bar), gui->status_label);

    GtkWidget *sp = gtk_label_new(""); gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(gui->status_bar), sp);

    gui->points_label = gtk_label_new("0 points");
    gtk_box_append(GTK_BOX(gui->status_bar), gui->points_label);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_append(GTK_BOX(gui->status_bar), sep);

    gui->time_label = gtk_label_new("--:--:--");
    gtk_box_append(GTK_BOX(gui->status_bar), gui->time_label);

    gui->clock_timer_id = g_timeout_add(1000, tick_clock, gui);
}

/* ============================================================================
 * Window Assembly
 * ========================================================================= */
void iec104_gui_init(IEC104_Gui *gui, GtkApplication *app,
                     IEC104_StationConfig *configs, int num_configs)
{
    memset(gui, 0, sizeof(*gui));
    gui->app = app;
    gui->configs = configs;
    gui->num_configs = num_configs;
    gui->active_conn = 0;
}

void iec104_gui_build(IEC104_Gui *gui)
{
    load_css();

    gui->window = gtk_application_window_new(gui->app);
    gtk_window_set_title(GTK_WINDOW(gui->window), "IEC 104 Client");
    gtk_window_set_default_size(GTK_WINDOW(gui->window), 1280, 720);

    build_header(gui);
    gtk_window_set_titlebar(GTK_WINDOW(gui->window), gui->header);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(gui->window), vbox);

    /* Horizontal paned: sidebar | content */
    GtkWidget *hp = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_paned_set_position(GTK_PANED(hp), 240);
    gtk_widget_set_vexpand(hp, TRUE);
    gtk_box_append(GTK_BOX(vbox), hp);

    /* ---- Sidebar ---- */
    build_sidebar(gui);
    GtkWidget *sf = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(sf, "sidebar");
    gtk_widget_set_size_request(sf, 200, -1);

    GtkWidget *sh = gtk_label_new("DATA GROUPS");
    gtk_widget_add_css_class(sh, "sidebar-header");
    gtk_widget_set_halign(sh, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(sf), sh);

    GtkWidget *ss = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ss),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ss), gui->sidebar_tree);
    gtk_widget_set_vexpand(ss, TRUE);
    gtk_box_append(GTK_BOX(sf), ss);

    gtk_paned_set_start_child(GTK_PANED(hp), sf);
    gtk_paned_set_resize_start_child(GTK_PANED(hp), FALSE);
    gtk_paned_set_shrink_start_child(GTK_PANED(hp), FALSE);

    /* ================================================================
     * Right side: vpaned [notebook | log]
     * ================================================================ */
    GtkWidget *vp = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_paned_set_position(GTK_PANED(vp), 480);

    gui->notebook = gtk_notebook_new();
    gtk_notebook_set_tab_pos(GTK_NOTEBOOK(gui->notebook), GTK_POS_TOP);
    gtk_widget_set_vexpand(gui->notebook, TRUE);
    gtk_widget_set_hexpand(gui->notebook, TRUE);
    gtk_widget_add_css_class(gui->notebook, "main-notebook");

    /* ---- Tab 1: Monitoring (Data Grid) ---- */
    GtkWidget *mon_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_add_css_class(mon_box, "data-grid");

    build_data_grid(gui);
    GtkWidget *gs = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gs),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(gs), gui->data_tree);
    gtk_widget_set_vexpand(gs, TRUE);
    gtk_widget_set_hexpand(gs, TRUE);
    gtk_box_append(GTK_BOX(mon_box), gs);

    GtkWidget *mon_label = gtk_label_new("\xf0\x9f\x93\x8a Monitoring");
    gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), mon_box, mon_label);

    /* ---- Tab 2: Commands (Command Control + QOC) ---- */
    GtkWidget *cmd_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *cmd_section_hdr = gtk_label_new("COMMAND CONTROL");
    gtk_widget_add_css_class(cmd_section_hdr, "section-header");
    gtk_widget_set_halign(cmd_section_hdr, GTK_ALIGN_START);
    gtk_widget_set_margin_start(cmd_section_hdr, 8);
    gtk_widget_set_margin_top(cmd_section_hdr, 6);
    gtk_box_append(GTK_BOX(cmd_box), cmd_section_hdr);

    GtkWidget *cmd_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(cmd_row, "section-row");
    gtk_widget_set_halign(cmd_row, GTK_ALIGN_START);

    build_cmd_panel(gui);
    gtk_box_append(GTK_BOX(cmd_row), gui->cmd_frame);

    build_qoc_panel(gui);
    gtk_box_append(GTK_BOX(cmd_row), gui->cmd_qoc_frame);

    gtk_box_append(GTK_BOX(cmd_box), cmd_row);

    /* Wrap in scrolled window for small screens */
    GtkWidget *cmd_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(cmd_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(cmd_scroll), cmd_box);
    gtk_widget_set_vexpand(cmd_scroll, TRUE);

    GtkWidget *cmd_label = gtk_label_new("\xe2\x9a\xa1 Commands");
    gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), cmd_scroll, cmd_label);

    /* Data grid row click → populate command panel */
    g_signal_connect(gui->data_tree, "row-activated",
                     G_CALLBACK(on_data_row_activated), gui);

    /* ---- Tab 3: System (System Commands) ---- */
    GtkWidget *sys_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    GtkWidget *sys_section_hdr = gtk_label_new("SYSTEM COMMANDS");
    gtk_widget_add_css_class(sys_section_hdr, "section-header");
    gtk_widget_set_halign(sys_section_hdr, GTK_ALIGN_START);
    gtk_widget_set_margin_start(sys_section_hdr, 8);
    gtk_widget_set_margin_top(sys_section_hdr, 6);
    gtk_box_append(GTK_BOX(sys_box), sys_section_hdr);

    GtkWidget *sys_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_add_css_class(sys_row, "section-row");
    gtk_widget_set_halign(sys_row, GTK_ALIGN_START);

    build_sys_panel(gui);
    gtk_box_append(GTK_BOX(sys_row), gui->sys_frame);
    GtkWidget *sys_right = (GtkWidget *)g_object_get_data(
        G_OBJECT(gui->sys_frame), "sys-right");
    gtk_box_append(GTK_BOX(sys_row), sys_right);

    gtk_box_append(GTK_BOX(sys_box), sys_row);

    /* Wrap in scrolled window */
    GtkWidget *sys_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sys_scroll),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(sys_scroll), sys_box);
    gtk_widget_set_vexpand(sys_scroll, TRUE);

    GtkWidget *sys_label = gtk_label_new("\xf0\x9f\x94\xa7 System");
    gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), sys_scroll, sys_label);

    /* ---- Tab 4: SLD (Single-Line Diagram) ---- */
    sld_init(&gui->sld, gui);
    GtkWidget *sld_widget = sld_build_widget(&gui->sld);
    GtkWidget *sld_label = gtk_label_new("\xf0\x9f\x94\x8c SLD");
    gtk_notebook_append_page(GTK_NOTEBOOK(gui->notebook), sld_widget, sld_label);

    /* ---- Notebook into top of vpane ---- */
    gtk_paned_set_start_child(GTK_PANED(vp), gui->notebook);
    gtk_paned_set_resize_start_child(GTK_PANED(vp), TRUE);

    /* ---- Log panel (bottom of vpane, visible on all tabs) ---- */
    build_log_panel(gui);
    GtkWidget *lb = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *lh = gtk_label_new("COMMUNICATION LOG");
    gtk_widget_add_css_class(lh, "sidebar-header");
    gtk_widget_set_halign(lh, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(lb), lh);

    GtkWidget *ls = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(ls),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(ls), gui->log_view);
    gtk_widget_set_vexpand(ls, TRUE);
    gtk_box_append(GTK_BOX(lb), ls);
    gtk_widget_set_size_request(lb, -1, 100);

    gtk_paned_set_end_child(GTK_PANED(vp), lb);
    gtk_paned_set_resize_end_child(GTK_PANED(vp), FALSE);
    gtk_paned_set_shrink_end_child(GTK_PANED(vp), TRUE);

    /* ---- Attach vpane to right side of horizontal pane ---- */
    gtk_paned_set_end_child(GTK_PANED(hp), vp);
    gtk_paned_set_resize_end_child(GTK_PANED(hp), TRUE);

    /* Status bar */
    build_status_bar(gui);
    gtk_box_append(GTK_BOX(vbox), gui->status_bar);

    iec104_gui_populate_sidebar(gui);
    gtk_window_present(GTK_WINDOW(gui->window));

    iec104_gui_log_message(gui, LOG_INFO, "IEC 104 Client started.");
    char buf[64];
    snprintf(buf, sizeof(buf), "Loaded %d connection(s).", gui->num_configs);
    iec104_gui_log_message(gui, LOG_INFO, buf);

}

/* ============================================================================
 * Sidebar population
 * ========================================================================= */
void iec104_gui_populate_sidebar(IEC104_Gui *gui)
{
    gtk_tree_store_clear(gui->sidebar_store);
    for (int c = 0; c < gui->num_configs; c++) {
        const IEC104_StationConfig *cfg = &gui->configs[c];
        GtkTreeIter ci;
        char lbl[128];
        snprintf(lbl, sizeof(lbl), "%s  [%s:%d]",
                 cfg->name, cfg->server_ip, cfg->server_port);
        gtk_tree_store_append(gui->sidebar_store, &ci, NULL);
        gtk_tree_store_set(gui->sidebar_store, &ci,
            SIDE_COL_ICON, "S", SIDE_COL_NAME, lbl,
            SIDE_COL_CONN_IDX, c, SIDE_COL_GROUP_NAME, "", -1);

        for (int g = 0; g < cfg->group_count; g++) {
            const IEC104_DataGroup *grp = &cfg->groups[g];
            GtkTreeIter gi;
            char gl[128];
            snprintf(gl, sizeof(gl), "%s  (%s, %d pts)",
                     grp->group_name, iec104_asdu_type_name(grp->asdu_type),
                     grp->point_count);
            gtk_tree_store_append(gui->sidebar_store, &gi, &ci);
            gtk_tree_store_set(gui->sidebar_store, &gi,
                SIDE_COL_ICON, "G", SIDE_COL_NAME, gl,
                SIDE_COL_CONN_IDX, c, SIDE_COL_GROUP_NAME, grp->group_name, -1);
        }
    }
    gtk_tree_view_expand_all(GTK_TREE_VIEW(gui->sidebar_tree));
}

/* ============================================================================
 * Data grid population
 * ========================================================================= */
void iec104_gui_populate_data(IEC104_Gui *gui, int conn_idx,
                              const char *group_name)
{
    gtk_list_store_clear(gui->data_store);
    if (conn_idx < 0 || conn_idx >= gui->num_configs) return;
    const IEC104_StationConfig *cfg = &gui->configs[conn_idx];
    int total = 0;

    for (int g = 0; g < cfg->group_count; g++) {
        const IEC104_DataGroup *grp = &cfg->groups[g];
        if (group_name && group_name[0] &&
            strcmp(grp->group_name, group_name) != 0) continue;

        for (int p = 0; p < grp->point_count; p++) {
            const IEC104_DataPoint *pt = &grp->points[p];
            char vs[64] = "---";
            switch (pt->value.type) {
            case VAL_BOOL:
                snprintf(vs, sizeof(vs), "%s", pt->value.v.b ? "ON" : "OFF"); break;
            case VAL_INT32: case VAL_DOUBLE_POINT:
                snprintf(vs, sizeof(vs), "%d", pt->value.v.i); break;
            case VAL_FLOAT:
                snprintf(vs, sizeof(vs), "%.4f", pt->value.v.f); break;
            case VAL_UINT32:
                snprintf(vs, sizeof(vs), "0x%08X", pt->value.v.bits); break;
            default: break;
            }

            const char *qs = (pt->quality == QUALITY_GOOD) ? "GOOD" :
                             (pt->quality == QUALITY_BAD)  ? "BAD"  : "---";

            char ts[32] = "---";
            if (pt->timestamp_ms > 0) {
                time_t s = (time_t)(pt->timestamp_ms / 1000);
                struct tm *t = localtime(&s);
                if (t) strftime(ts, sizeof(ts), "%H:%M:%S", t);
            }

            GtkTreeIter it;
            gtk_list_store_append(gui->data_store, &it);
            gtk_list_store_set(gui->data_store, &it,
                DATA_COL_IOA, pt->ioa,
                DATA_COL_NAME, pt->name,
                DATA_COL_TYPE, iec104_asdu_type_name(pt->asdu_type),
                DATA_COL_VALUE, vs,
                DATA_COL_QUALITY, qs,
                DATA_COL_TIMESTAMP, ts,
                DATA_COL_CONN_IDX, conn_idx, -1);
            total++;
        }
    }

    char pb[32]; snprintf(pb, sizeof(pb), "%d points", total);
    gtk_label_set_text(GTK_LABEL(gui->points_label), pb);
}

/* ============================================================================
 * Status update
 * ========================================================================= */
void iec104_gui_update_status(IEC104_Gui *gui, int conn_idx,
                               IEC104_ConnState state)
{
    if (conn_idx != gui->active_conn) return;
    const char *pt, *pc;
    const char *rm[] = { "status-connected", "status-disconnected",
                         "status-connecting", NULL };

    switch (state) {
    case CONN_CONNECTED:    pt = "CONNECTED";    pc = "status-connected";    break;
    case CONN_CONNECTING:
    case CONN_RECONNECTING: pt = "CONNECTING";   pc = "status-connecting";   break;
    default:                pt = "DISCONNECTED"; pc = "status-disconnected"; break;
    }

    for (int i = 0; rm[i]; i++)
        gtk_widget_remove_css_class(gui->status_pill, rm[i]);
    gtk_widget_add_css_class(gui->status_pill, pc);
    gtk_label_set_text(GTK_LABEL(gui->status_pill), pt);

    if (conn_idx >= 0 && conn_idx < gui->num_configs) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%s  %s:%d",
                 gui->configs[conn_idx].name,
                 gui->configs[conn_idx].server_ip,
                 gui->configs[conn_idx].server_port);
        gtk_label_set_text(GTK_LABEL(gui->status_label), buf);
    }
}

/* ============================================================================
 * Log message
 * ========================================================================= */
void iec104_gui_log_message(IEC104_Gui *gui, IEC104_LogLevel level,
                             const char *msg)
{
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[16]; strftime(ts, sizeof(ts), "%H:%M:%S", t);

    const char *tag;
    switch (level) {
    case LOG_SUCCESS: tag = "success"; break;
    case LOG_WARNING: tag = "warning"; break;
    case LOG_ERROR:   tag = "error";   break;
    case LOG_COMMAND: tag = "command"; break;
    default:          tag = "info";    break;
    }

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(gui->log_buffer, &end);
    char line[384];
    snprintf(line, sizeof(line), "[%s] %s\n", ts, msg);
    gtk_text_buffer_insert_with_tags_by_name(gui->log_buffer, &end,
                                              line, -1, tag, NULL);

    GtkTextMark *mark = gtk_text_buffer_get_insert(gui->log_buffer);
    gtk_text_buffer_get_end_iter(gui->log_buffer, &end);
    gtk_text_buffer_move_mark(gui->log_buffer, mark, &end);
    gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(gui->log_view), mark);
}

void iec104_gui_refresh_data(IEC104_Gui *gui, int conn_idx)
{
    if (conn_idx != gui->active_conn) return;
    iec104_gui_populate_data(gui, conn_idx,
                             gui->active_group[0] ? gui->active_group : NULL);
}

/* ============================================================================
 * Edit Slave Dialog (Phase 4)
 * ========================================================================= */
typedef struct {
    IEC104_Gui *gui;
    int         conn_idx;
    GtkWidget  *name_entry;
    GtkWidget  *ip_entry;
    GtkWidget  *port_entry;
    GtkWidget  *coa_entry;
    /* Group editing rows */
    GtkWidget  *groups_box;
    int         group_count;
    GtkWidget  *gn_entries[IEC104_MAX_GROUPS];
    GtkWidget  *gt_entries[IEC104_MAX_GROUPS];
    GtkWidget  *gs_entries[IEC104_MAX_GROUPS];
    GtkWidget  *gq_entries[IEC104_MAX_GROUPS];
} EditSlaveCtx;

/* All ASDU type names for the dropdown */
static const char *asdu_type_list[] = {
    "M_SP_NA_1", "M_SP_TA_1", "M_DP_NA_1", "M_DP_TA_1",
    "M_ST_NA_1", "M_BO_NA_1", "M_ME_NA_1", "M_ME_NB_1",
    "M_ME_NC_1", "M_IT_NA_1", "M_SP_TB_1", "M_DP_TB_1",
    "M_ST_TB_1", "M_BO_TB_1", "M_ME_TD_1", "M_ME_TE_1",
    "M_ME_TF_1", "M_IT_TB_1", "M_EI_NA_1",
    "C_SC_NA_1", "C_DC_NA_1", "C_RC_NA_1",
    "C_SE_NA_1", "C_SE_NB_1", "C_SE_NC_1", "C_BO_NA_1",
    "C_SC_TA_1", "C_DC_TA_1", "C_RC_TA_1",
    "C_SE_TA_1", "C_SE_TB_1", "C_SE_TC_1", "C_BO_TA_1",
    "C_IC_NA_1", "C_CI_NA_1", "C_CS_NA_1",
    NULL
};

/* Find index of a type name in asdu_type_list, default 0 */
static guint find_asdu_type_index(const char *name)
{
    for (guint i = 0; asdu_type_list[i]; i++) {
        if (strcmp(asdu_type_list[i], name) == 0) return i;
    }
    return 0;
}

/* Get selected type name from a GtkDropDown */
static const char* get_dropdown_type(GtkWidget *dd)
{
    GtkStringObject *obj = GTK_STRING_OBJECT(
        gtk_drop_down_get_selected_item(GTK_DROP_DOWN(dd)));
    return obj ? gtk_string_object_get_string(obj) : "M_SP_NA_1";
}

/* Build column header row above group rows */
static GtkWidget* build_group_header(void)
{
    GtkWidget *hdr = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_margin_bottom(hdr, 2);

    const char *titles[]  = { "Name", "Type", "Start IOA", "Qty", "" };
    const int   widths[]  = { 120,     140,    100,          100,    32 };
    const int   margins[] = {   0,       0,     30,           10,     0 };
    const int   ncols = 5;

    for (int i = 0; i < ncols; i++) {
        GtkWidget *lbl = gtk_label_new(titles[i]);
        gtk_widget_set_size_request(lbl, widths[i], -1);
        gtk_widget_set_halign(lbl, GTK_ALIGN_START);
        gtk_widget_set_margin_start(lbl, margins[i]);
        gtk_widget_add_css_class(lbl, "sidebar-header");
        gtk_box_append(GTK_BOX(hdr), lbl);
    }
    return hdr;
}

static void on_remove_group_btn(GtkButton *b, gpointer d)
{
    /* Hide the row — the group name entry is cleared so on_edit_save skips it */
    GtkWidget *row = gtk_widget_get_parent(GTK_WIDGET(b));
    if (row) {
        /* Clear the first entry (group name) so save logic skips this row */
        GtkWidget *child = gtk_widget_get_first_child(row);
        if (child && GTK_IS_ENTRY(child))
            gtk_editable_set_text(GTK_EDITABLE(child), "");
        gtk_widget_set_visible(row, FALSE);
    }
    (void)d;
}

static void add_group_row(EditSlaveCtx *ctx, const char *name,
                          const char *type, int start_ioa, int qty)
{
    int i = ctx->group_count;
    if (i >= IEC104_MAX_GROUPS) return;

    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    /* Name entry */
    ctx->gn_entries[i] = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->gn_entries[i]), name);
    gtk_widget_set_size_request(ctx->gn_entries[i], 120, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->gn_entries[i]), "Group name");
    gtk_box_append(GTK_BOX(row), ctx->gn_entries[i]);

    /* Type dropdown */
    GtkStringList *type_model = gtk_string_list_new(asdu_type_list);
    ctx->gt_entries[i] = gtk_drop_down_new(G_LIST_MODEL(type_model), NULL);
    gtk_drop_down_set_selected(GTK_DROP_DOWN(ctx->gt_entries[i]),
                               find_asdu_type_index(type));
    gtk_widget_set_size_request(ctx->gt_entries[i], 140, -1);
    gtk_box_append(GTK_BOX(row), ctx->gt_entries[i]);

    /* Start IOA entry */
    char sb[16]; snprintf(sb, sizeof(sb), "%d", start_ioa);
    ctx->gs_entries[i] = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->gs_entries[i]), sb);
    gtk_widget_set_size_request(ctx->gs_entries[i], 60, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->gs_entries[i]), "Start");
    gtk_box_append(GTK_BOX(row), ctx->gs_entries[i]);

    /* Qty entry */
    char qb[16]; snprintf(qb, sizeof(qb), "%d", qty);
    ctx->gq_entries[i] = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->gq_entries[i]), qb);
    gtk_widget_set_size_request(ctx->gq_entries[i], 50, -1);
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->gq_entries[i]), "Qty");
    gtk_box_append(GTK_BOX(row), ctx->gq_entries[i]);

    /* Remove button */
    GtkWidget *rm_btn = gtk_button_new_with_label("✕");
    gtk_widget_add_css_class(rm_btn, "btn-remove");
    g_signal_connect(rm_btn, "clicked", G_CALLBACK(on_remove_group_btn), ctx);
    gtk_box_append(GTK_BOX(row), rm_btn);

    gtk_box_append(GTK_BOX(ctx->groups_box), row);
    ctx->group_count++;
}

static void on_add_group_btn(GtkButton *b, gpointer d)
{
    (void)b;
    add_group_row((EditSlaveCtx *)d, "", "M_SP_NA_1", 1, 10);
}

static void on_edit_save(GtkButton *b, gpointer d)
{
    EditSlaveCtx *ctx = (EditSlaveCtx *)d; (void)b;
    IEC104_StationConfig *cfg = &ctx->gui->configs[ctx->conn_idx];

    snprintf(cfg->name, sizeof(cfg->name), "%s",
             gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry)));
    snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s",
             gtk_editable_get_text(GTK_EDITABLE(ctx->ip_entry)));
    cfg->server_port = atoi(gtk_editable_get_text(
                            GTK_EDITABLE(ctx->port_entry)));
    cfg->common_address = atoi(gtk_editable_get_text(
                            GTK_EDITABLE(ctx->coa_entry)));

    /* Rebuild groups */
    cfg->group_count = 0;
    for (int i = 0; i < ctx->group_count; i++) {
        const char *gn = gtk_editable_get_text(GTK_EDITABLE(ctx->gn_entries[i]));
        const char *gt = get_dropdown_type(ctx->gt_entries[i]);
        int si = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->gs_entries[i])));
        int qt = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->gq_entries[i])));

        if (gn[0] == '\0' || qt <= 0) continue;

        IEC104_DataGroup grp;
        iec104_group_init(&grp, gn, iec104_asdu_type_from_string(gt), si, qt, "");
        iec104_station_add_group(cfg, &grp);
    }

    /* Close dialog first so it doesn't block */
    GtkWidget *win = gtk_widget_get_ancestor(GTK_WIDGET(b), GTK_TYPE_WINDOW);
    int ci = ctx->conn_idx;
    IEC104_Gui *gui = ctx->gui;
    if (win) gtk_window_destroy(GTK_WINDOW(win));
    g_free(ctx);

    /* Auto-save to JSON + reconnect if config changed */
    iec104_gui_apply_config(gui, ci);
}

void iec104_gui_show_edit_slave(IEC104_Gui *gui)
{
    if (gui->active_conn < 0 || gui->active_conn >= gui->num_configs) return;
    int ci = gui->active_conn;
    IEC104_StationConfig *cfg = &gui->configs[ci];

    EditSlaveCtx *ctx = g_new0(EditSlaveCtx, 1);
    ctx->gui = gui;
    ctx->conn_idx = ci;

    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "Edit Connection");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 400);
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(gui->window));
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(win), vbox);

    /* Connection fields */
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    ctx->name_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), cfg->name);
    gtk_widget_set_hexpand(ctx->name_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ctx->name_entry, 1, 0, 3, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("IP:"), 0, 1, 1, 1);
    ctx->ip_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->ip_entry), cfg->server_ip);
    gtk_grid_attach(GTK_GRID(grid), ctx->ip_entry, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Port:"), 2, 1, 1, 1);
    ctx->port_entry = gtk_entry_new();
    char pb[8]; snprintf(pb, sizeof(pb), "%d", cfg->server_port);
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), pb);
    gtk_widget_set_size_request(ctx->port_entry, 60, -1);
    gtk_grid_attach(GTK_GRID(grid), ctx->port_entry, 3, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("COA:"), 0, 2, 1, 1);
    ctx->coa_entry = gtk_entry_new();
    char cb[8]; snprintf(cb, sizeof(cb), "%d", cfg->common_address);
    gtk_editable_set_text(GTK_EDITABLE(ctx->coa_entry), cb);
    gtk_widget_set_size_request(ctx->coa_entry, 60, -1);
    gtk_grid_attach(GTK_GRID(grid), ctx->coa_entry, 1, 2, 1, 1);

    gtk_box_append(GTK_BOX(vbox), grid);

    /* Groups section title */
    GtkWidget *glbl = gtk_label_new("Data Groups");
    gtk_widget_set_halign(glbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(glbl, "sidebar-header");
    gtk_box_append(GTK_BOX(vbox), glbl);

    /* Column headers */
    gtk_box_append(GTK_BOX(vbox), build_group_header());

    /* Groups list */
    ctx->groups_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *gscroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gscroll),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(gscroll), ctx->groups_box);
    gtk_widget_set_vexpand(gscroll, TRUE);
    gtk_box_append(GTK_BOX(vbox), gscroll);

    /* Populate existing groups */
    for (int g = 0; g < cfg->group_count; g++) {
        add_group_row(ctx, cfg->groups[g].group_name,
                      iec104_asdu_type_name(cfg->groups[g].asdu_type),
                      cfg->groups[g].start_ioa, cfg->groups[g].point_count);
    }

    /* Buttons */
    GtkWidget *btnbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Group");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_group_btn), ctx);
    gtk_box_append(GTK_BOX(btnbox), add_btn);

    GtkWidget *sp = gtk_label_new(""); gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(btnbox), sp);

    GtkWidget *save_btn = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(save_btn, "toolbar-btn-connect");
    g_signal_connect(save_btn, "clicked", G_CALLBACK(on_edit_save), ctx);
    gtk_box_append(GTK_BOX(btnbox), save_btn);

    gtk_box_append(GTK_BOX(vbox), btnbox);

    gtk_window_present(GTK_WINDOW(win));
}

/* ============================================================================
 * New Connection Dialog (Phase 4)
 * ========================================================================= */
static void on_new_conn_create(GtkButton *b, gpointer d)
{
    EditSlaveCtx *ctx = (EditSlaveCtx *)d; (void)b;
    IEC104_Gui *gui = ctx->gui;

    if (gui->num_configs >= IEC104_MAX_CONNECTIONS) {
        iec104_gui_log_message(gui, LOG_ERROR, "Max connections reached.");
        GtkWidget *win = gtk_widget_get_ancestor(GTK_WIDGET(b), GTK_TYPE_WINDOW);
        if (win) gtk_window_destroy(GTK_WINDOW(win));
        g_free(ctx);
        return;
    }

    int ci = gui->num_configs;
    IEC104_StationConfig *cfg = &gui->configs[ci];
    iec104_station_init(cfg,
        gtk_editable_get_text(GTK_EDITABLE(ctx->name_entry)),
        atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->coa_entry))),
        gtk_editable_get_text(GTK_EDITABLE(ctx->ip_entry)),
        atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->port_entry))));

    /* Add groups */
    for (int i = 0; i < ctx->group_count; i++) {
        const char *gn = gtk_editable_get_text(GTK_EDITABLE(ctx->gn_entries[i]));
        const char *gt = get_dropdown_type(ctx->gt_entries[i]);
        int si = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->gs_entries[i])));
        int qt = atoi(gtk_editable_get_text(GTK_EDITABLE(ctx->gq_entries[i])));
        if (gn[0] && qt > 0) {
            IEC104_DataGroup grp;
            iec104_group_init(&grp, gn, iec104_asdu_type_from_string(gt), si, qt, "");
            iec104_station_add_group(cfg, &grp);
        }
    }

    gui->num_configs++;

    /* Close dialog first */
    GtkWidget *win = gtk_widget_get_ancestor(GTK_WIDGET(b), GTK_TYPE_WINDOW);
    if (win) gtk_window_destroy(GTK_WINDOW(win));
    g_free(ctx);

    /* Auto-save to JSON + init connection + auto-connect */
    iec104_gui_apply_config(gui, ci);
}

void iec104_gui_show_new_connection(IEC104_Gui *gui)
{
    EditSlaveCtx *ctx = g_new0(EditSlaveCtx, 1);
    ctx->gui = gui;
    ctx->conn_idx = gui->num_configs;  /* new index */

    GtkWidget *win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(win), "New Connection");
    gtk_window_set_default_size(GTK_WINDOW(win), 520, 400);
    gtk_window_set_transient_for(GTK_WINDOW(win), GTK_WINDOW(gui->window));
    gtk_window_set_modal(GTK_WINDOW(win), TRUE);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_widget_set_margin_start(vbox, 16);
    gtk_widget_set_margin_end(vbox, 16);
    gtk_widget_set_margin_top(vbox, 16);
    gtk_widget_set_margin_bottom(vbox, 16);
    gtk_window_set_child(GTK_WINDOW(win), vbox);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Name:"), 0, 0, 1, 1);
    ctx->name_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->name_entry), "New_Station");
    gtk_widget_set_hexpand(ctx->name_entry, TRUE);
    gtk_grid_attach(GTK_GRID(grid), ctx->name_entry, 1, 0, 3, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("IP:"), 0, 1, 1, 1);
    ctx->ip_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->ip_entry), "127.0.0.1");
    gtk_grid_attach(GTK_GRID(grid), ctx->ip_entry, 1, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Port:"), 2, 1, 1, 1);
    ctx->port_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->port_entry), "2404");
    gtk_widget_set_size_request(ctx->port_entry, 60, -1);
    gtk_grid_attach(GTK_GRID(grid), ctx->port_entry, 3, 1, 1, 1);

    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("COA:"), 0, 2, 1, 1);
    ctx->coa_entry = gtk_entry_new();
    gtk_editable_set_text(GTK_EDITABLE(ctx->coa_entry), "1");
    gtk_widget_set_size_request(ctx->coa_entry, 60, -1);
    gtk_grid_attach(GTK_GRID(grid), ctx->coa_entry, 1, 2, 1, 1);

    gtk_box_append(GTK_BOX(vbox), grid);

    /* Groups section title */
    GtkWidget *glbl = gtk_label_new("Data Groups");
    gtk_widget_set_halign(glbl, GTK_ALIGN_START);
    gtk_widget_add_css_class(glbl, "sidebar-header");
    gtk_box_append(GTK_BOX(vbox), glbl);

    /* Column headers */
    gtk_box_append(GTK_BOX(vbox), build_group_header());

    ctx->groups_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *gs = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(gs),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(gs), ctx->groups_box);
    gtk_widget_set_vexpand(gs, TRUE);
    gtk_box_append(GTK_BOX(vbox), gs);

    /* Default group */
    add_group_row(ctx, "Group-1", "M_SP_NA_1", 1, 10);

    GtkWidget *btnbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *add_btn = gtk_button_new_with_label("+ Add Group");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_add_group_btn), ctx);
    gtk_box_append(GTK_BOX(btnbox), add_btn);

    GtkWidget *sp = gtk_label_new(""); gtk_widget_set_hexpand(sp, TRUE);
    gtk_box_append(GTK_BOX(btnbox), sp);

    GtkWidget *create_btn = gtk_button_new_with_label("Create");
    gtk_widget_add_css_class(create_btn, "toolbar-btn-connect");
    g_signal_connect(create_btn, "clicked", G_CALLBACK(on_new_conn_create), ctx);
    gtk_box_append(GTK_BOX(btnbox), create_btn);

    gtk_box_append(GTK_BOX(vbox), btnbox);
    gtk_window_present(GTK_WINDOW(win));
}

/* ============================================================================
 * Phase 5: Save Config
 * ========================================================================= */
void iec104_gui_save_config(IEC104_Gui *gui)
{
    if (gui->config_path[0] == '\0') {
        snprintf(gui->config_path, sizeof(gui->config_path),
                 "station_config.json");
    }

    int ret = iec104_config_save(gui->config_path, gui->configs, gui->num_configs);
    if (ret == 0) {
        char msg[320];
        snprintf(msg, sizeof(msg), "Config saved to %s", gui->config_path);
        iec104_gui_log_message(gui, LOG_SUCCESS, msg);
    } else {
        iec104_gui_log_message(gui, LOG_ERROR, "Failed to save config!");
    }
}

/* ============================================================================
 * Phase 5: Load Config — open JSON file → stop all → reload → reconnect
 *
 * Pipeline:
 *   1. Parse the JSON file into a temporary buffer
 *   2. Validate: reject if parsing fails (keep previous config)
 *   3. Stop all active connections
 *   4. Replace in-memory configs with the new ones
 *   5. Reinitialize connection objects with callbacks
 *   6. Update config_path for future saves
 *   7. Auto-connect all connections
 *   8. Refresh sidebar + data grid
 * ========================================================================= */
void iec104_gui_load_config(IEC104_Gui *gui, const char *filepath)
{
    extern void cb_state_change(const char *, IEC104_ConnState, void *);
    extern void cb_point_update(const char *, int, IEC104_Value,
                                IEC104_Quality, uint64_t, int, void *);
    extern void cb_log(const char *, IEC104_LogLevel, const char *, void *);

    if (!filepath || !filepath[0]) {
        iec104_gui_log_message(gui, LOG_ERROR, "No file path specified.");
        return;
    }

    /* --- Step 1: Parse new config into temp buffer --- */
    /* MUST be static — IEC104_StationConfig is ~775 KB each, stack overflow otherwise */
    static IEC104_StationConfig tmp_configs[IEC104_MAX_CONNECTIONS];
    int new_count = iec104_config_load(filepath, tmp_configs,
                                        IEC104_MAX_CONNECTIONS);

    /* --- Step 2: Validate --- */
    if (new_count <= 0) {
        char msg[320];
        snprintf(msg, sizeof(msg),
                 "Failed to load config from '%s'. Keeping current config.",
                 filepath);
        iec104_gui_log_message(gui, LOG_ERROR, msg);
        return;
    }

    /* --- Step 3: Stop all active connections --- */
    if (gui->connections) {
        for (int i = 0; i < gui->num_configs; i++) {
            IEC104_Connection *c = &gui->connections[i];
            if (c->state != CONN_DISCONNECTED) {
                iec104_conn_stop(c);
            }
        }
        /* Let connection threads shut down */
        for (int retry = 0; retry < 30; retry++) {
            bool all_stopped = true;
            for (int i = 0; i < gui->num_configs; i++) {
                if (gui->connections[i].running) {
                    all_stopped = false;
                    break;
                }
            }
            if (all_stopped) break;
            g_main_context_iteration(NULL, FALSE);
        }
    }

    /* --- Step 4: Replace in-memory configs --- */
    for (int i = 0; i < new_count; i++)
        gui->configs[i] = tmp_configs[i];
    gui->num_configs = new_count;
    gui->active_conn = 0;
    gui->active_group[0] = '\0';

    /* --- Step 5: Reinitialize connection objects --- */
    if (gui->connections) {
        for (int i = 0; i < gui->num_configs; i++) {
            IEC104_StationConfig *cfg = &gui->configs[i];
            iec104_conn_init(&gui->connections[i], cfg->name, cfg);
            gui->connections[i].on_state_change        = cb_state_change;
            gui->connections[i].on_point_update         = cb_point_update;
            gui->connections[i].on_log                  = cb_log;
            gui->connections[i].cb_user_data            = (void *)(intptr_t)i;
            gui->connections[i].auto_time_sync          = true;
            gui->connections[i].time_sync_interval_sec  = 3600;
        }
    }

    /* --- Step 6: Update config path --- */
    snprintf(gui->config_path, sizeof(gui->config_path), "%s", filepath);

    /* --- Step 7: Auto-connect all --- */
    iec104_gui_connect_all(gui);

    /* --- Step 8: Refresh UI --- */
    iec104_gui_populate_sidebar(gui);
    if (gui->num_configs > 0)
        iec104_gui_populate_data(gui, 0, NULL);

    char msg[320];
    snprintf(msg, sizeof(msg), "Loaded %d connection(s) from '%s'.",
             gui->num_configs, filepath);
    iec104_gui_log_message(gui, LOG_SUCCESS, msg);
}

/* ============================================================================
 * Phase 5: Apply Config — save → validate → diff → reconnect
 *
 * This is the single source of truth workflow:
 *   1. Save gui->configs[] to station_config.json
 *   2. Re-parse the file to validate JSON integrity
 *   3. Compare old vs new config for the affected connection
 *   4. If changed: stop → reinit → start (STARTDT + GI)
 *   5. Refresh sidebar + monitoring grid
 *
 * Returns 0 on success, -1 on error (previous config preserved).
 * ========================================================================= */
int iec104_gui_apply_config(IEC104_Gui *gui, int changed_conn_idx)
{
    extern void cb_state_change(const char *, IEC104_ConnState, void *);
    extern void cb_point_update(const char *, int, IEC104_Value,
                                IEC104_Quality, uint64_t, int, void *);
    extern void cb_log(const char *, IEC104_LogLevel, const char *, void *);

    if (changed_conn_idx < 0 || changed_conn_idx >= gui->num_configs) {
        iec104_gui_log_message(gui, LOG_ERROR,
            "apply_config: invalid connection index.");
        return -1;
    }

    /* --- Step 1: Determine config path --- */
    if (gui->config_path[0] == '\0')
        snprintf(gui->config_path, sizeof(gui->config_path),
                 "station_config.json");

    /* --- Step 2: Save current config to JSON --- */
    int ret = iec104_config_save(gui->config_path, gui->configs,
                                  gui->num_configs);
    if (ret != 0) {
        iec104_gui_log_message(gui, LOG_ERROR,
            "apply_config: failed to save config file!");
        return -1;
    }
    iec104_gui_log_message(gui, LOG_SUCCESS, "Config saved.");

    /* --- Step 3: Validate by re-parsing --- */
    /* MUST be static — IEC104_StationConfig is ~775 KB each */
    static IEC104_StationConfig tmp_configs[IEC104_MAX_CONNECTIONS];
    int tmp_count = iec104_config_load(gui->config_path, tmp_configs,
                                        IEC104_MAX_CONNECTIONS);
    if (tmp_count <= 0) {
        iec104_gui_log_message(gui, LOG_ERROR,
            "apply_config: saved JSON is invalid! Keeping previous config.");
        /* Re-save the previous valid config to overwrite the bad file */
        iec104_config_save(gui->config_path, gui->configs, gui->num_configs);
        return -1;
    }

    /* --- Step 4: Change detection --- */
    bool need_reconnect = true;
    if (changed_conn_idx < tmp_count) {
        /* Compare old (reloaded) vs current in-memory config.
         * If they match, no reconnect is needed. */
        if (iec104_station_config_equals(&tmp_configs[changed_conn_idx],
                                          &gui->configs[changed_conn_idx])) {
            need_reconnect = false;
        }
    }

    /* --- Step 5: Stop → reinit → start the affected connection --- */
    if (need_reconnect && gui->connections) {
        int ci = changed_conn_idx;
        IEC104_Connection *c = &gui->connections[ci];

        /* Stop the connection if running */
        if (c->state != CONN_DISCONNECTED && c->state != CONN_ERROR) {
            iec104_conn_stop(c);
            iec104_gui_log_message(gui, LOG_INFO,
                "Disconnecting for config reload...");
            /* Give connection thread time to stop */
            for (int w = 0; w < 20 && c->running; w++)
                g_main_context_iteration(NULL, FALSE);
        }

        /* Reinitialize the connection with updated config */
        IEC104_StationConfig *cfg = &gui->configs[ci];
        iec104_conn_init(c, cfg->name, cfg);
        c->on_state_change        = cb_state_change;
        c->on_point_update        = cb_point_update;
        c->on_log                 = cb_log;
        c->cb_user_data           = (void *)(intptr_t)ci;
        c->auto_time_sync         = true;
        c->time_sync_interval_sec = 3600;

        /* Start (TCP connect → STARTDT → GI) */
        iec104_conn_start(c);

        char msg[128];
        snprintf(msg, sizeof(msg), "Connection '%s' restarted with updated config.",
                 cfg->name);
        iec104_gui_log_message(gui, LOG_SUCCESS, msg);
    }

    /* --- Step 6: Refresh UI --- */
    iec104_gui_populate_sidebar(gui);
    if (gui->active_conn == changed_conn_idx) {
        iec104_gui_populate_data(gui, changed_conn_idx, NULL);
        gui->active_group[0] = '\0';
    }

    return 0;
}

/* ============================================================================
 * Phase 5: Export CSV — dumps data grid to a CSV file
 * ========================================================================= */
void iec104_gui_export_csv(IEC104_Gui *gui)
{
    /* Generate timestamped filename */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char fname[128];
    strftime(fname, sizeof(fname), "export_%Y%m%d_%H%M%S.csv", t);

    FILE *fp = fopen(fname, "w");
    if (!fp) {
        iec104_gui_log_message(gui, LOG_ERROR, "Failed to create export file!");
        return;
    }

    fprintf(fp, "Connection,Group,IOA,Name,ASDU_Type,Value,Quality,Timestamp\n");

    for (int c = 0; c < gui->num_configs; c++) {
        const IEC104_StationConfig *cfg = &gui->configs[c];
        for (int g = 0; g < cfg->group_count; g++) {
            const IEC104_DataGroup *grp = &cfg->groups[g];
            for (int p = 0; p < grp->point_count; p++) {
                const IEC104_DataPoint *pt = &grp->points[p];

                char vs[64] = "";
                switch (pt->value.type) {
                case VAL_BOOL:
                    snprintf(vs, sizeof(vs), "%d", pt->value.v.b); break;
                case VAL_INT32: case VAL_DOUBLE_POINT:
                    snprintf(vs, sizeof(vs), "%d", pt->value.v.i); break;
                case VAL_FLOAT:
                    snprintf(vs, sizeof(vs), "%.6f", pt->value.v.f); break;
                case VAL_UINT32:
                    snprintf(vs, sizeof(vs), "0x%08X", pt->value.v.bits); break;
                default: break;
                }

                const char *qs = (pt->quality == QUALITY_GOOD) ? "GOOD" :
                                 (pt->quality == QUALITY_BAD)  ? "BAD"  : "UNKNOWN";

                char ts[32] = "";
                if (pt->timestamp_ms > 0) {
                    time_t s = (time_t)(pt->timestamp_ms / 1000);
                    struct tm *tm = localtime(&s);
                    if (tm) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
                }

                fprintf(fp, "\"%s\",\"%s\",%d,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"\n",
                        cfg->name, grp->group_name, pt->ioa, pt->name,
                        iec104_asdu_type_name(pt->asdu_type),
                        vs, qs, ts);
            }
        }
    }

    fclose(fp);

    char msg[256];
    snprintf(msg, sizeof(msg), "Exported to %s", fname);
    iec104_gui_log_message(gui, LOG_SUCCESS, msg);
}

/* ============================================================================
 * Phase 5: Connect All / Disconnect All
 * ========================================================================= */
void iec104_gui_connect_all(IEC104_Gui *gui)
{
    if (!gui->connections) return;
    int started = 0;
    for (int i = 0; i < gui->num_configs; i++) {
        IEC104_Connection *c = &gui->connections[i];
        if (c->state == CONN_DISCONNECTED || c->state == CONN_ERROR) {
            iec104_conn_start(c);
            started++;
        }
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "Connecting %d connection(s)...", started);
    iec104_gui_log_message(gui, LOG_INFO, msg);
}

void iec104_gui_disconnect_all(IEC104_Gui *gui)
{
    if (!gui->connections) return;
    int stopped = 0;
    for (int i = 0; i < gui->num_configs; i++) {
        IEC104_Connection *c = &gui->connections[i];
        if (c->state != CONN_DISCONNECTED) {
            iec104_conn_stop(c);
            iec104_gui_update_status(gui, i, CONN_DISCONNECTED);
            stopped++;
        }
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "Disconnected %d connection(s).", stopped);
    iec104_gui_log_message(gui, LOG_WARNING, msg);
}
