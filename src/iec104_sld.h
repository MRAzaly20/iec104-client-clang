/**
 * @file iec104_sld.h
 * @brief Single-Line Diagram — Double Bus Single Breaker Scheme
 *
 * Layout per feeder bay (matching IEC standard):
 *   Bus-II ─── DS-xB ──┐
 *                       ├─ Junction ── DS-xD (bypass)
 *   Bus-I  ─── DS-xA ──┘
 *                       │
 *                      CB-x
 *                       │
 *                     DS-xC
 *                       │
 *                      [ES]
 *                       │
 *                    Feeder / XFMR
 */
#ifndef IEC104_SLD_H
#define IEC104_SLD_H

#include <gtk/gtk.h>
#include <stdbool.h>

typedef struct _IEC104_Gui  IEC104_Gui;

#define SLD_MAX_BAYS    8

/* ============================================================================
 * SLD Element Types
 * ========================================================================= */

typedef struct {
    const char *label;
    int         ioa;      /**< IOA (0 = not used / skip) */
    bool        closed;
    double      x, y;
    double      w, h;
} SLD_Switch;

typedef struct {
    const char *label;
    const char *unit;
    int         ioa;
    float       value;
    double      x, y;
} SLD_Measurement;

typedef enum {
    BAY_OHL,
    BAY_XFMR,
    BAY_COUPLER,
} SLD_BayType;

/**
 * Double Bus Single Breaker bay.
 * Switches per bay:
 *   ds_b  = Bus-II selector disconnector
 *   ds_a  = Bus-I  selector disconnector
 *   ds_d  = Bypass disconnector (horizontal)
 *   cb    = Circuit Breaker
 *   ds_c  = Line disconnector
 *   es    = Earth Switch
 */
typedef struct {
    const char     *name;
    SLD_BayType     type;
    SLD_Switch      ds_b;       /**< Bus-II selector */
    SLD_Switch      ds_a;       /**< Bus-I  selector */
    SLD_Switch      ds_d;       /**< Bypass disconnector */
    SLD_Switch      cb;         /**< Circuit Breaker */
    SLD_Switch      ds_c;       /**< Line disconnector */
    SLD_Switch      es;         /**< Earth Switch */
    SLD_Measurement meas[3];
    int             meas_count;
    bool            energized;
    double          cx;         /**< Bay column center X */
} SLD_Bay;

typedef struct {
    SLD_Bay     bays[SLD_MAX_BAYS];
    int         bay_count;
    double      bus_ii_y;
    double      bus_i_y;
    GtkWidget  *drawing_area;
    IEC104_Gui *gui;
} SLD_State;

/* ============================================================================
 * Public API
 * ========================================================================= */
void       sld_init(SLD_State *sld, IEC104_Gui *gui);
GtkWidget* sld_build_widget(SLD_State *sld);
bool       sld_update_switch(SLD_State *sld, int ioa, bool closed);
bool       sld_update_measurement(SLD_State *sld, int ioa, float value);
void       sld_update_topology(SLD_State *sld);
void       sld_queue_redraw(SLD_State *sld);

#endif /* IEC104_SLD_H */
