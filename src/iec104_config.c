/**
 * @file iec104_config.c
 * @brief JSON configuration load/save via cJSON
 */
#include "iec104_config.h"
#include "iec104_model.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * ASDU type string → enum
 * ========================================================================= */
typedef struct { const char *name; IEC104_AsduType type; } AsduEntry;

static const AsduEntry s_asdu_table[] = {
    {"M_SP_NA_1", ASDU_M_SP_NA_1}, {"M_SP_TA_1", ASDU_M_SP_TA_1},
    {"M_DP_NA_1", ASDU_M_DP_NA_1}, {"M_DP_TA_1", ASDU_M_DP_TA_1},
    {"M_ST_NA_1", ASDU_M_ST_NA_1}, {"M_ST_TA_1", ASDU_M_ST_TA_1},
    {"M_BO_NA_1", ASDU_M_BO_NA_1}, {"M_BO_TA_1", ASDU_M_BO_TA_1},
    {"M_ME_NA_1", ASDU_M_ME_NA_1}, {"M_ME_TA_1", ASDU_M_ME_TA_1},
    {"M_ME_NB_1", ASDU_M_ME_NB_1}, {"M_ME_TB_1", ASDU_M_ME_TB_1},
    {"M_ME_NC_1", ASDU_M_ME_NC_1}, {"M_ME_TC_1", ASDU_M_ME_TC_1},
    {"M_IT_NA_1", ASDU_M_IT_NA_1}, {"M_IT_TA_1", ASDU_M_IT_TA_1},
    {"M_EP_TA_1", ASDU_M_EP_TA_1}, {"M_EP_TB_1", ASDU_M_EP_TB_1},
    {"M_EP_TC_1", ASDU_M_EP_TC_1}, {"M_PS_NA_1", ASDU_M_PS_NA_1},
    {"M_EP_TD_1", ASDU_M_EP_TD_1}, {"M_EP_TE_1", ASDU_M_EP_TE_1},
    {"M_EP_TF_1", ASDU_M_EP_TF_1},
    {"M_ME_ND_1", ASDU_M_ME_ND_1},
    {"M_SP_TB_1", ASDU_M_SP_TB_1}, {"M_DP_TB_1", ASDU_M_DP_TB_1},
    {"M_ST_TB_1", ASDU_M_ST_TB_1}, {"M_BO_TB_1", ASDU_M_BO_TB_1},
    {"M_ME_TD_1", ASDU_M_ME_TD_1}, {"M_ME_TE_1", ASDU_M_ME_TE_1},
    {"M_ME_TF_1", ASDU_M_ME_TF_1}, {"M_IT_TB_1", ASDU_M_IT_TB_1},
    {"M_EI_NA_1", ASDU_M_EI_NA_1},
    {"C_SC_NA_1", ASDU_C_SC_NA_1}, {"C_DC_NA_1", ASDU_C_DC_NA_1},
    {"C_RC_NA_1", ASDU_C_RC_NA_1}, {"C_SE_NA_1", ASDU_C_SE_NA_1},
    {"C_SE_NB_1", ASDU_C_SE_NB_1}, {"C_SE_NC_1", ASDU_C_SE_NC_1},
    {"C_BO_NA_1", ASDU_C_BO_NA_1},
    {"C_SC_TA_1", ASDU_C_SC_TA_1}, {"C_DC_TA_1", ASDU_C_DC_TA_1},
    {"C_RC_TA_1", ASDU_C_RC_TA_1}, {"C_SE_TA_1", ASDU_C_SE_TA_1},
    {"C_SE_TB_1", ASDU_C_SE_TB_1}, {"C_SE_TC_1", ASDU_C_SE_TC_1},
    {"C_BO_TA_1", ASDU_C_BO_TA_1},
    {"C_IC_NA_1", ASDU_C_IC_NA_1}, {"C_CI_NA_1", ASDU_C_CI_NA_1},
    {"C_RD_NA_1", ASDU_C_RD_NA_1}, {"C_CS_NA_1", ASDU_C_CS_NA_1},
    {"C_TS_NA_1", ASDU_C_TS_NA_1}, {"C_RP_NA_1", ASDU_C_RP_NA_1},
    {"C_CD_NA_1", ASDU_C_CD_NA_1}, {"C_TS_TA_1", ASDU_C_TS_TA_1},
    {NULL, 0}
};

IEC104_AsduType iec104_asdu_type_from_string(const char *str)
{
    if (!str) return (IEC104_AsduType)0;
    for (int i = 0; s_asdu_table[i].name; i++) {
        if (strcmp(s_asdu_table[i].name, str) == 0)
            return s_asdu_table[i].type;
    }
    return (IEC104_AsduType)0;
}

/* ============================================================================
 * Internal: read whole file into malloc'd buffer
 * ========================================================================= */
static char* read_file(const char *path, long *out_size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)(sz + 1));
    if (!buf) { fclose(f); return NULL; }

    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);

    if (out_size) *out_size = sz;
    return buf;
}

/* ============================================================================
 * Internal: parse a single station JSON object
 * ========================================================================= */
static int parse_station(const cJSON *obj, IEC104_StationConfig *cfg)
{
    const cJSON *j;

    j = cJSON_GetObjectItem(obj, "name");
    const char *name = (j && j->valuestring) ? j->valuestring : "Station";

    j = cJSON_GetObjectItem(obj, "common_address");
    int ca = j ? j->valueint : 1;

    j = cJSON_GetObjectItem(obj, "server_ip");
    const char *ip = (j && j->valuestring) ? j->valuestring : "127.0.0.1";

    j = cJSON_GetObjectItem(obj, "server_port");
    int port = j ? j->valueint : 2404;

    iec104_station_init(cfg, name, ca, ip, port);

    /* Parse groups array */
    const cJSON *groups_arr = cJSON_GetObjectItem(obj, "groups");
    if (!groups_arr || !cJSON_IsArray(groups_arr))
        return 0;  /* station is valid even without groups */

    const cJSON *gj;
    cJSON_ArrayForEach(gj, groups_arr) {
        if (cfg->group_count >= IEC104_MAX_GROUPS)
            break;

        const cJSON *gn = cJSON_GetObjectItem(gj, "group_name");
        const cJSON *at = cJSON_GetObjectItem(gj, "asdu_type");
        const cJSON *si = cJSON_GetObjectItem(gj, "start_ioa");
        const cJSON *qt = cJSON_GetObjectItem(gj, "quantity");
        const cJSON *ds = cJSON_GetObjectItem(gj, "description");

        if (!gn || !gn->valuestring || !at || !at->valuestring || !si || !qt)
            continue;

        IEC104_AsduType atype = iec104_asdu_type_from_string(at->valuestring);
        if ((int)atype == 0) {
            fprintf(stderr, "[CONFIG] Unknown ASDU type '%s' — skipped\n",
                    at->valuestring);
            continue;
        }

        IEC104_DataGroup grp;
        iec104_group_init(&grp, gn->valuestring, atype,
                          si->valueint, qt->valueint,
                          (ds && ds->valuestring) ? ds->valuestring : "");

        iec104_station_add_group(cfg, &grp);
    }
    return 0;
}

/* ============================================================================
 * Public: load config from JSON file
 * ========================================================================= */
int iec104_config_load(const char *filepath,
                       IEC104_StationConfig *configs, int max_configs)
{
    long sz = 0;
    char *buf = read_file(filepath, &sz);
    if (!buf) {
        fprintf(stderr, "[CONFIG] Cannot open: %s\n", filepath);
        return -1;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        fprintf(stderr, "[CONFIG] JSON parse error near: %.40s\n",
                err ? err : "?");
        return -1;
    }

    int count = 0;

    /* Multi-connection format: {"connections": [...]} */
    cJSON *conns = cJSON_GetObjectItem(root, "connections");
    if (conns && cJSON_IsArray(conns)) {
        cJSON *entry;
        cJSON_ArrayForEach(entry, conns) {
            if (count >= max_configs) break;
            if (parse_station(entry, &configs[count]) == 0)
                count++;
        }
    }
    /* Old-style single-station format (bare object with "name" key) */
    else if (cJSON_IsObject(root) && cJSON_GetObjectItem(root, "name")) {
        if (parse_station(root, &configs[0]) == 0)
            count = 1;
    }
    else {
        fprintf(stderr, "[CONFIG] Unrecognised JSON structure\n");
    }

    cJSON_Delete(root);
    return count;
}

/* ============================================================================
 * Public: save config to JSON file
 * ========================================================================= */
int iec104_config_save(const char *filepath,
                       const IEC104_StationConfig *configs, int count)
{
    cJSON *root = cJSON_CreateObject();
    if (!root) return -1;

    cJSON *arr = cJSON_AddArrayToObject(root, "connections");

    for (int c = 0; c < count; c++) {
        const IEC104_StationConfig *cfg = &configs[c];
        cJSON *st = cJSON_CreateObject();
        cJSON_AddStringToObject(st, "name", cfg->name);
        cJSON_AddNumberToObject(st, "common_address", cfg->common_address);
        cJSON_AddStringToObject(st, "server_ip", cfg->server_ip);
        cJSON_AddNumberToObject(st, "server_port", cfg->server_port);

        cJSON *ga = cJSON_AddArrayToObject(st, "groups");
        for (int g = 0; g < cfg->group_count; g++) {
            const IEC104_DataGroup *grp = &cfg->groups[g];
            cJSON *go = cJSON_CreateObject();
            cJSON_AddStringToObject(go, "group_name", grp->group_name);
            cJSON_AddStringToObject(go, "asdu_type",
                                   iec104_asdu_type_name(grp->asdu_type));
            cJSON_AddNumberToObject(go, "start_ioa", grp->start_ioa);
            cJSON_AddNumberToObject(go, "quantity", grp->quantity);
            cJSON_AddStringToObject(go, "description", grp->description);
            cJSON_AddItemToArray(ga, go);
        }
        cJSON_AddItemToArray(arr, st);
    }

    char *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    if (!json_str) return -1;

    FILE *f = fopen(filepath, "w");
    if (!f) { free(json_str); return -1; }
    fputs(json_str, f);
    fclose(f);
    free(json_str);
    return 0;
}
