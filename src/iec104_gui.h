/**
 * @file iec104_gui.h
 * @brief GTK4 GUI for IEC 104 Client
 */
#ifndef IEC104_GUI_H
#define IEC104_GUI_H

#include <gtk/gtk.h>
#include "iec104_types.h"
#include "iec104_conn.h"
#include "iec104_sld.h"

/* ============================================================================
 * GUI Application Context
 * ========================================================================= */
typedef struct _IEC104_Gui {
    GtkApplication       *app;
    GtkWidget            *window;

    /* Header bar */
    GtkWidget            *header;
    GtkWidget            *btn_connect;
    GtkWidget            *btn_disconnect;
    GtkWidget            *btn_gi;
    GtkWidget            *btn_sync;
    GtkWidget            *btn_edit;
    GtkWidget            *btn_new_conn;
    GtkWidget            *btn_save_cfg;
    GtkWidget            *btn_load_cfg;
    GtkWidget            *btn_export;
    GtkWidget            *btn_connect_all;
    GtkWidget            *btn_disconnect_all;

    /* Sidebar */
    GtkWidget            *sidebar_tree;
    GtkTreeStore         *sidebar_store;

    /* Data grid */
    GtkWidget            *data_tree;
    GtkListStore         *data_store;

    /* Tabbed notebook */
    GtkWidget            *notebook;

    /* ---- Command Control Panel (redesigned) ---- */
    GtkWidget            *cmd_outer_box;       /* hbox: cmd_frame | qoc_frame   */

    /* Left: Command Control frame */
    GtkWidget            *cmd_frame;           /* outer frame                   */
    GtkWidget            *cmd_type_combo;       /* ASDU type dropdown            */
    GtkWidget            *cmd_type_badge;       /* type badge label              */
    GtkWidget            *cmd_target_frame;     /* TARGET info box               */
    GtkWidget            *cmd_ioa_label;        /* "IOA: X  Name"                */
    GtkWidget            *cmd_status_dot;       /* green/red status dot          */
    GtkWidget            *cmd_qu_combo;         /* QU qualifier dropdown         */
    GtkWidget            *cmd_se_combo;         /* S/E (execute / select)        */
    GtkWidget            *cmd_time_check;       /* with CP56Time2a               */
    GtkWidget            *cmd_value_entry;      /* value input (setpoint/BO)     */
    GtkWidget            *cmd_value_row;        /* value row (hide/show)         */
    GtkWidget            *cmd_range_label;      /* range hint label              */
    GtkWidget            *cmd_action_box;       /* container for action buttons  */
    GtkWidget            *cmd_status_label;     /* result label                  */
    GtkWidget            *cmd_ioa_entry;        /* IOA manual entry              */

    /* Right: QOC Reference info panel */
    GtkWidget            *cmd_qoc_frame;        /* QOC info frame                */

    /* System commands panel */
    GtkWidget            *sys_frame;
    GtkWidget            *sys_read_ioa;    /* IOA entry for Read command */
    GtkWidget            *sys_delay_entry; /* ms entry for Delay Acquisition */

    /* Status bar */
    GtkWidget            *status_bar;
    GtkWidget            *status_label;
    GtkWidget            *status_pill;
    GtkWidget            *points_label;
    GtkWidget            *time_label;

    /* Log panel */
    GtkWidget            *log_view;
    GtkTextBuffer        *log_buffer;

    /* Data */
    IEC104_StationConfig *configs;
    int                   num_configs;
    IEC104_Connection    *connections;
    int                   active_conn;
    char                  active_group[IEC104_MAX_NAME_LEN];
    char                  config_path[260];  /* for save/reload */

    guint                 clock_timer_id;
    bool                  data_dirty;       /* GI flood coalescing flag */

    /* Command panel state */
    int                   cmd_selected_ioa;
    char                  cmd_selected_name[IEC104_MAX_NAME_LEN];
    int                   cmd_current_type;  /* index into cmd_type_names */

    /* SLD (Single-Line Diagram) */
    SLD_State             sld;

} IEC104_Gui;

/* ---- Lifecycle ---- */
void iec104_gui_init(IEC104_Gui *gui, GtkApplication *app,
                     IEC104_StationConfig *configs, int num_configs);
void iec104_gui_build(IEC104_Gui *gui);

/* ---- Updates (thread-safe via g_idle_add) ---- */
void iec104_gui_update_status(IEC104_Gui *gui, int conn_idx, IEC104_ConnState state);
void iec104_gui_refresh_data(IEC104_Gui *gui, int conn_idx);
void iec104_gui_log_message(IEC104_Gui *gui, IEC104_LogLevel level, const char *msg);
void iec104_gui_populate_sidebar(IEC104_Gui *gui);
void iec104_gui_populate_data(IEC104_Gui *gui, int conn_idx, const char *group_name);

/* ---- Dialogs ---- */
void iec104_gui_show_edit_slave(IEC104_Gui *gui);
void iec104_gui_show_new_connection(IEC104_Gui *gui);

/* ---- Phase 5: Integration ---- */
void iec104_gui_save_config(IEC104_Gui *gui);
void iec104_gui_load_config(IEC104_Gui *gui, const char *filepath);
void iec104_gui_export_csv(IEC104_Gui *gui);
void iec104_gui_connect_all(IEC104_Gui *gui);
void iec104_gui_disconnect_all(IEC104_Gui *gui);
int  iec104_gui_apply_config(IEC104_Gui *gui, int changed_conn_idx);

#endif /* IEC104_GUI_H */
