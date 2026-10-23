/**
 * @file iec104_model.c
 * @brief Data model implementation for DataPoint, DataGroup, StationConfig
 */
#include "iec104_model.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * ASDU type name lookup
 * ========================================================================= */
const char* iec104_asdu_type_name(IEC104_AsduType type)
{
    switch (type) {
    case ASDU_M_SP_NA_1: return "M_SP_NA_1";
    case ASDU_M_SP_TA_1: return "M_SP_TA_1";
    case ASDU_M_DP_NA_1: return "M_DP_NA_1";
    case ASDU_M_DP_TA_1: return "M_DP_TA_1";
    case ASDU_M_ST_NA_1: return "M_ST_NA_1";
    case ASDU_M_ST_TA_1: return "M_ST_TA_1";
    case ASDU_M_BO_NA_1: return "M_BO_NA_1";
    case ASDU_M_BO_TA_1: return "M_BO_TA_1";
    case ASDU_M_ME_NA_1: return "M_ME_NA_1";
    case ASDU_M_ME_TA_1: return "M_ME_TA_1";
    case ASDU_M_ME_NB_1: return "M_ME_NB_1";
    case ASDU_M_ME_TB_1: return "M_ME_TB_1";
    case ASDU_M_ME_NC_1: return "M_ME_NC_1";
    case ASDU_M_ME_TC_1: return "M_ME_TC_1";
    case ASDU_M_IT_NA_1: return "M_IT_NA_1";
    case ASDU_M_IT_TA_1: return "M_IT_TA_1";
    case ASDU_M_EP_TA_1: return "M_EP_TA_1";
    case ASDU_M_EP_TB_1: return "M_EP_TB_1";
    case ASDU_M_EP_TC_1: return "M_EP_TC_1";
    case ASDU_M_PS_NA_1: return "M_PS_NA_1";
    case ASDU_M_ME_ND_1: return "M_ME_ND_1";
    case ASDU_M_SP_TB_1: return "M_SP_TB_1";
    case ASDU_M_DP_TB_1: return "M_DP_TB_1";
    case ASDU_M_ST_TB_1: return "M_ST_TB_1";
    case ASDU_M_BO_TB_1: return "M_BO_TB_1";
    case ASDU_M_ME_TD_1: return "M_ME_TD_1";
    case ASDU_M_ME_TE_1: return "M_ME_TE_1";
    case ASDU_M_ME_TF_1: return "M_ME_TF_1";
    case ASDU_M_IT_TB_1: return "M_IT_TB_1";
    case ASDU_M_EP_TD_1: return "M_EP_TD_1";
    case ASDU_M_EP_TE_1: return "M_EP_TE_1";
    case ASDU_M_EP_TF_1: return "M_EP_TF_1";
    case ASDU_M_EI_NA_1: return "M_EI_NA_1";
    case ASDU_C_SC_NA_1: return "C_SC_NA_1";
    case ASDU_C_DC_NA_1: return "C_DC_NA_1";
    case ASDU_C_RC_NA_1: return "C_RC_NA_1";
    case ASDU_C_SE_NA_1: return "C_SE_NA_1";
    case ASDU_C_SE_NB_1: return "C_SE_NB_1";
    case ASDU_C_SE_NC_1: return "C_SE_NC_1";
    case ASDU_C_BO_NA_1: return "C_BO_NA_1";
    case ASDU_C_SC_TA_1: return "C_SC_TA_1";
    case ASDU_C_DC_TA_1: return "C_DC_TA_1";
    case ASDU_C_RC_TA_1: return "C_RC_TA_1";
    case ASDU_C_SE_TA_1: return "C_SE_TA_1";
    case ASDU_C_SE_TB_1: return "C_SE_TB_1";
    case ASDU_C_SE_TC_1: return "C_SE_TC_1";
    case ASDU_C_BO_TA_1: return "C_BO_TA_1";
    case ASDU_C_IC_NA_1: return "C_IC_NA_1";
    case ASDU_C_CI_NA_1: return "C_CI_NA_1";
    case ASDU_C_RD_NA_1: return "C_RD_NA_1";
    case ASDU_C_CS_NA_1: return "C_CS_NA_1";
    case ASDU_C_TS_NA_1: return "C_TS_NA_1";
    case ASDU_C_RP_NA_1: return "C_RP_NA_1";
    case ASDU_C_CD_NA_1: return "C_CD_NA_1";
    case ASDU_C_TS_TA_1: return "C_TS_TA_1";
    default:             return "UNKNOWN";
    }
}

/* Map ASDU types to their base "family" so that time-tag variants match.
 * E.g. M_SP_NA_1(1), M_SP_TA_1(2), M_SP_TB_1(30) → family 1 (Single Point)  */
int iec104_type_family(int type)
{
    switch (type) {
    case 1: case 2: case 30:              return 1;   /* Single Point  */
    case 3: case 4: case 31:              return 3;   /* Double Point  */
    case 5: case 6: case 32:              return 5;   /* Step Position */
    case 7: case 8: case 33:              return 7;   /* Bitstring     */
    case 9: case 10: case 34:             return 9;   /* Meas. Norm.   */
    case 11: case 12: case 35:            return 11;  /* Meas. Scaled  */
    case 13: case 14: case 36:            return 13;  /* Meas. Float   */
    case 15: case 16: case 37:            return 15;  /* Int. Totals   */
    case 21:                              return 9;   /* Norm. no qual → same family as Norm. */
    case 20:                              return 20;  /* Packed SCD    */
    case 17: case 38:                    return 17;  /* Prot. Event   */
    case 18: case 39:                    return 18;  /* Prot. Packed Start */
    case 19: case 40:                    return 19;  /* Prot. Output Circuit */
    default:                              return type; /* exact match   */
    }
}

bool iec104_is_monitoring_type(IEC104_AsduType type)
{
    return (type >= 1 && type <= 21) || (type >= 30 && type <= 40) || (type == 70);
}

bool iec104_is_command_type(IEC104_AsduType type)
{
    return (type >= 45 && type <= 64) || (type >= 100 && type <= 107);
}

const char* iec104_conn_state_name(IEC104_ConnState state)
{
    switch (state) {
    case CONN_DISCONNECTED: return "DISCONNECTED";
    case CONN_CONNECTING:   return "CONNECTING";
    case CONN_CONNECTED:    return "CONNECTED";
    case CONN_RECONNECTING: return "RECONNECTING";
    case CONN_ERROR:        return "ERROR";
    case CONN_TIMEOUT:      return "TIMEOUT";
    default:                return "UNKNOWN";
    }
}

/* ============================================================================
 * DataPoint
 * ========================================================================= */
void iec104_point_init(IEC104_DataPoint *pt, int ioa, const char *name,
                       IEC104_AsduType type)
{
    memset(pt, 0, sizeof(*pt));
    pt->ioa = ioa;
    pt->asdu_type = type;
    pt->quality = QUALITY_UNKNOWN;
    pt->value.type = VAL_NONE;
    if (name)
        snprintf(pt->name, sizeof(pt->name), "%s", name);
}

void iec104_point_update(IEC104_DataPoint *pt, IEC104_Value val,
                         IEC104_Quality q, uint64_t ts_ms)
{
    pt->value   = val;
    pt->quality = q;
    pt->timestamp_ms = ts_ms;
    pt->updated = true;
}

/* ============================================================================
 * DataGroup
 * ========================================================================= */
void iec104_group_init(IEC104_DataGroup *grp, const char *name,
                       IEC104_AsduType type, int start_ioa,
                       int quantity, const char *desc)
{
    memset(grp, 0, sizeof(*grp));
    snprintf(grp->group_name, sizeof(grp->group_name), "%s", name);
    grp->asdu_type = type;
    grp->start_ioa = start_ioa;
    grp->quantity  = quantity;
    if (desc)
        snprintf(grp->description, sizeof(grp->description), "%s", desc);

    /* Cap quantity to compile-time limit */
    int cap = quantity;
    if (cap > IEC104_MAX_POINTS_PER_GRP)
        cap = IEC104_MAX_POINTS_PER_GRP;
    grp->point_count = cap;

    /* Auto-generate DataPoint instances */
    for (int i = 0; i < cap; i++) {
        char pt_name[IEC104_MAX_NAME_LEN];
        snprintf(pt_name, sizeof(pt_name), "%s_%d", name, i + 1);
        iec104_point_init(&grp->points[i], start_ioa + i, pt_name, type);
    }
}

IEC104_DataPoint* iec104_group_get_point(IEC104_DataGroup *grp, int ioa)
{
    for (int i = 0; i < grp->point_count; i++) {
        if (grp->points[i].ioa == ioa)
            return &grp->points[i];
    }
    return NULL;
}

int iec104_group_end_ioa(const IEC104_DataGroup *grp)
{
    return grp->start_ioa + grp->quantity - 1;
}

/* ============================================================================
 * StationConfig
 * ========================================================================= */
void iec104_station_init(IEC104_StationConfig *cfg, const char *name, int ca,
                         const char *ip, int port)
{
    memset(cfg, 0, sizeof(*cfg));
    snprintf(cfg->name, sizeof(cfg->name), "%s", name);
    cfg->common_address = ca;
    snprintf(cfg->server_ip, sizeof(cfg->server_ip), "%s",
             ip ? ip : "127.0.0.1");
    cfg->server_port = (port > 0) ? port : 2404;
}

bool iec104_groups_overlap(const IEC104_DataGroup *a, const IEC104_DataGroup *b)
{
    int end_a = a->start_ioa + a->quantity - 1;
    int end_b = b->start_ioa + b->quantity - 1;
    return !(end_a < b->start_ioa || end_b < a->start_ioa);
}

int iec104_station_add_group(IEC104_StationConfig *cfg,
                             const IEC104_DataGroup *grp)
{
    if (cfg->group_count >= IEC104_MAX_GROUPS)
        return -1;  /* full */

    cfg->groups[cfg->group_count] = *grp;
    cfg->group_count++;
    return 0;
}

IEC104_DataPoint* iec104_station_find_point(IEC104_StationConfig *cfg, int ioa)
{
    for (int g = 0; g < cfg->group_count; g++) {
        IEC104_DataPoint *pt = iec104_group_get_point(&cfg->groups[g], ioa);
        if (pt) return pt;
    }
    return NULL;
}

IEC104_DataPoint* iec104_station_find_point_by_type(IEC104_StationConfig *cfg,
                                                     int ioa, IEC104_AsduType type)
{
    int family = iec104_type_family((int)type);
    for (int g = 0; g < cfg->group_count; g++) {
        if (iec104_type_family((int)cfg->groups[g].asdu_type) != family)
            continue;
        IEC104_DataPoint *pt = iec104_group_get_point(&cfg->groups[g], ioa);
        if (pt) return pt;
    }
    return NULL;
}

IEC104_DataGroup* iec104_station_find_group(IEC104_StationConfig *cfg,
                                            const char *name)
{
    for (int g = 0; g < cfg->group_count; g++) {
        if (strcmp(cfg->groups[g].group_name, name) == 0)
            return &cfg->groups[g];
    }
    return NULL;
}

int iec104_station_total_points(const IEC104_StationConfig *cfg)
{
    int total = 0;
    for (int g = 0; g < cfg->group_count; g++)
        total += cfg->groups[g].point_count;
    return total;
}

/* ------------------------------------------------------------------
 * Debug: print config to stdout
 * ------------------------------------------------------------------ */
void iec104_station_print(const IEC104_StationConfig *cfg)
{
    printf("Station: %s\n", cfg->name);
    printf("  COA : %d\n", cfg->common_address);
    printf("  Addr: %s:%d\n", cfg->server_ip, cfg->server_port);
    printf("  Groups: %d  |  Total points: %d\n",
           cfg->group_count, iec104_station_total_points(cfg));

    for (int g = 0; g < cfg->group_count; g++) {
        const IEC104_DataGroup *grp = &cfg->groups[g];
        printf("    [%d] %-20s  %s  IOA %d–%d  (%d pts)",
               g, grp->group_name,
               iec104_asdu_type_name(grp->asdu_type),
               grp->start_ioa, iec104_group_end_ioa(grp),
               grp->point_count);
        if (grp->description[0])
            printf("  \"%s\"", grp->description);
        printf("\n");
    }
}

/* ============================================================================
 * Change detection: compare two station configs for meaningful differences
 * ========================================================================= */
bool iec104_station_config_equals(const IEC104_StationConfig *a,
                                   const IEC104_StationConfig *b)
{
    if (strcmp(a->server_ip, b->server_ip) != 0) return false;
    if (a->server_port != b->server_port)       return false;
    if (a->common_address != b->common_address) return false;
    if (a->group_count != b->group_count)       return false;
    for (int g = 0; g < a->group_count; g++) {
        const IEC104_DataGroup *ga = &a->groups[g];
        const IEC104_DataGroup *gb = &b->groups[g];
        if (ga->asdu_type != gb->asdu_type)     return false;
        if (ga->start_ioa != gb->start_ioa)     return false;
        if (ga->point_count != gb->point_count) return false;
        if (strcmp(ga->group_name, gb->group_name) != 0) return false;
    }
    return true;
}
