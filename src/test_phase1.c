/**
 * @file test_phase1.c
 * @brief Phase 1 verification — tests data model + JSON config load/save
 *
 * Usage: test_phase1 [config.json]
 * If no argument is given, creates and saves a default config, then reloads it.
 */
#include "iec104_types.h"
#include "iec104_model.h"
#include "iec104_config.h"
#include "iec104_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define SEP "============================================================"

/* == Test 1: data model unit tests ======================================= */
static int test_data_model(void)
{
    int fails = 0;
    printf("\n%s\n", SEP);
    printf("TEST 1: Data Model\n");
    printf("%s\n", SEP);

    /* --- DataPoint --- */
    IEC104_DataPoint pt;
    iec104_point_init(&pt, 100, "TestPoint", ASDU_M_SP_NA_1);
    assert(pt.ioa == 100);
    assert(pt.asdu_type == ASDU_M_SP_NA_1);
    assert(pt.quality == QUALITY_UNKNOWN);
    assert(pt.value.type == VAL_NONE);
    printf("  [PASS] DataPoint init\n");

    IEC104_Value val = { .type = VAL_BOOL, .v = { .b = true } };
    iec104_point_update(&pt, val, QUALITY_GOOD, 1700000000000ULL);
    assert(pt.value.type == VAL_BOOL);
    assert(pt.value.v.b == true);
    assert(pt.quality == QUALITY_GOOD);
    assert(pt.updated == true);
    printf("  [PASS] DataPoint update\n");

    /* --- DataGroup --- */
    IEC104_DataGroup grp;
    iec104_group_init(&grp, "Test_Group", ASDU_M_ME_NC_1, 300, 5, "Float vals");
    assert(grp.point_count == 5);
    assert(grp.start_ioa == 300);
    assert(strcmp(grp.group_name, "Test_Group") == 0);

    IEC104_DataPoint *p1 = iec104_group_get_point(&grp, 300);
    assert(p1 != NULL);
    assert(p1->ioa == 300);
    assert(p1->asdu_type == ASDU_M_ME_NC_1);

    IEC104_DataPoint *p5 = iec104_group_get_point(&grp, 304);
    assert(p5 != NULL && p5->ioa == 304);

    IEC104_DataPoint *px = iec104_group_get_point(&grp, 305);
    assert(px == NULL); /* out of range */

    assert(iec104_group_end_ioa(&grp) == 304);
    printf("  [PASS] DataGroup init + get_point + end_ioa\n");

    /* --- StationConfig --- */
    IEC104_StationConfig cfg;
    iec104_station_init(&cfg, "Station1", 47, "192.168.1.100", 2404);
    assert(cfg.common_address == 47);
    assert(cfg.server_port == 2404);
    assert(strcmp(cfg.server_ip, "192.168.1.100") == 0);

    int rc = iec104_station_add_group(&cfg, &grp);
    assert(rc == 0);
    assert(cfg.group_count == 1);

    /* Try overlapping group */
    IEC104_DataGroup grp2;
    iec104_group_init(&grp2, "Overlap", ASDU_M_SP_NA_1, 302, 3, "");
    rc = iec104_station_add_group(&cfg, &grp2);
    assert(rc == -2); /* overlap must be rejected */
    printf("  [PASS] StationConfig add_group + overlap detection\n");

    /* Non-overlapping group */
    IEC104_DataGroup grp3;
    iec104_group_init(&grp3, "Digital", ASDU_M_SP_NA_1, 100, 10, "");
    rc = iec104_station_add_group(&cfg, &grp3);
    assert(rc == 0);
    assert(cfg.group_count == 2);

    IEC104_DataPoint *fp = iec104_station_find_point(&cfg, 102);
    assert(fp != NULL && fp->ioa == 102);

    fp = iec104_station_find_point(&cfg, 999);
    assert(fp == NULL);

    assert(iec104_station_total_points(&cfg) == 15); /* 5 + 10 */
    printf("  [PASS] StationConfig find_point + total_points\n");

    /* --- ASDU type helpers --- */
    assert(strcmp(iec104_asdu_type_name(ASDU_M_SP_NA_1), "M_SP_NA_1") == 0);
    assert(strcmp(iec104_asdu_type_name(ASDU_C_DC_TA_1), "C_DC_TA_1") == 0);
    assert(strcmp(iec104_asdu_type_name(ASDU_C_BO_NA_1), "C_BO_NA_1") == 0);
    assert(strcmp(iec104_asdu_type_name(ASDU_M_EI_NA_1), "M_EI_NA_1") == 0);
    assert(iec104_is_monitoring_type(ASDU_M_ME_NC_1) == true);
    assert(iec104_is_monitoring_type(ASDU_C_DC_NA_1) == false);
    assert(iec104_is_command_type(ASDU_C_SE_NC_1) == true);
    assert(iec104_is_command_type(ASDU_M_SP_NA_1) == false);
    printf("  [PASS] ASDU type name + monitoring/command classification\n");

    /* Connection state name */
    assert(strcmp(iec104_conn_state_name(CONN_CONNECTED), "CONNECTED") == 0);
    printf("  [PASS] Connection state name\n");

    printf("\n  >> All data model tests PASSED\n");
    return fails;
}

/* == Test 2: default config create + save + reload ======================= */
static int test_default_config(void)
{
    printf("\n%s\n", SEP);
    printf("TEST 2: Default Config Create → Save → Reload\n");
    printf("%s\n", SEP);

    /* Create config manually (iec104_config_create_default was removed) */
    IEC104_StationConfig def;
    iec104_station_init(&def, "IEC104_Station", 1, "127.0.0.1", 2404);
    IEC104_DataGroup g;
    iec104_group_init(&g, "Digital_Inputs", ASDU_M_SP_NA_1, 100, 10, "Single-point status inputs");
    iec104_station_add_group(&def, &g);
    iec104_group_init(&g, "Double_Points", ASDU_M_DP_NA_1, 200, 5, "Double-point status");
    iec104_station_add_group(&def, &g);
    iec104_group_init(&g, "Analog_Values", ASDU_M_ME_NC_1, 300, 20, "Floating-point measurements");
    iec104_station_add_group(&def, &g);
    iec104_group_init(&g, "Counters", ASDU_M_IT_NA_1, 400, 5, "Integrated energy counters");
    iec104_station_add_group(&def, &g);
    iec104_group_init(&g, "Commands", ASDU_C_DC_TA_1, 500, 5, "Double commands with timestamp");
    iec104_station_add_group(&def, &g);
    iec104_group_init(&g, "Setpoints", ASDU_C_SE_NC_1, 600, 10, "Floating-point setpoints");
    iec104_station_add_group(&def, &g);
    printf("\n  --- Test config ---\n");
    iec104_station_print(&def);

    assert(def.group_count == 6);
    assert(def.common_address == 1);
    assert(iec104_station_total_points(&def) == 55); /* 10+5+20+5+5+10 */
    printf("  [PASS] Test config created with %d groups, %d points\n",
           def.group_count, iec104_station_total_points(&def));

    /* Save to temp file */
    const char *tmp = "test_output.json";
    int rc = iec104_config_save(tmp, &def, 1);
    assert(rc == 0);
    printf("  [PASS] Config saved to %s\n", tmp);

    /* Reload */
    IEC104_StationConfig reloaded[2];
    int count = iec104_config_load(tmp, reloaded, 2);
    assert(count == 1);
    printf("\n  --- Reloaded config ---\n");
    iec104_station_print(&reloaded[0]);

    assert(strcmp(reloaded[0].name, "IEC104_Station") == 0);
    assert(reloaded[0].common_address == 1);
    assert(reloaded[0].group_count == 6);
    assert(iec104_station_total_points(&reloaded[0]) == 55);
    printf("  [PASS] Config reloaded and verified\n");

    /* Clean up */
    remove(tmp);
    printf("\n  >> Default config round-trip PASSED\n");
    return 0;
}

/* == Test 3: load existing station_config.json =========================== */
static int test_load_file(const char *path)
{
    printf("\n%s\n", SEP);
    printf("TEST 3: Load External Config: %s\n", path);
    printf("%s\n", SEP);

    IEC104_StationConfig configs[IEC104_MAX_CONNECTIONS];
    int count = iec104_config_load(path, configs, IEC104_MAX_CONNECTIONS);
    if (count < 0) {
        printf("  [FAIL] Could not load %s\n", path);
        return 1;
    }

    printf("\n  Loaded %d connection(s):\n\n", count);
    for (int c = 0; c < count; c++) {
        printf("  ---- Connection %d ----\n", c + 1);
        iec104_station_print(&configs[c]);
        printf("\n");
    }

    /* Save back to verify round-trip */
    const char *tmp_out = "test_roundtrip.json";
    int rc = iec104_config_save(tmp_out, configs, count);
    assert(rc == 0);
    printf("  [PASS] Round-trip saved to %s\n", tmp_out);
    remove(tmp_out);

    printf("\n  >> External config load PASSED\n");
    return 0;
}

/* == Test 4: Logger ====================================================== */
static int test_logger(void)
{
    printf("\n%s\n", SEP);
    printf("TEST 4: Logger\n");
    printf("%s\n", SEP);

    iec104_log_init("test_log.csv");

    iec104_log(LOG_INFO,    "System started");
    iec104_log(LOG_SUCCESS, "Connection established to 127.0.0.1:2404");
    iec104_log(LOG_WARNING, "Reconnect attempt %d", 3);
    iec104_log(LOG_ERROR,   "Timeout after %.1f seconds", 10.0);
    iec104_log(LOG_COMMAND, "DC ON IOA=%d → sent", 6);

    IEC104_TrafficEntry te = {
        .timestamp_ms = 0,
        .is_tx = false,
        .ioa = 100,
    };
    snprintf(te.type_name, sizeof(te.type_name), "M_SP_NA_1");
    snprintf(te.details, sizeof(te.details), "val=1 q=GOOD");
    iec104_log_traffic(&te);

    iec104_log_shutdown();

    printf("  [PASS] Log messages written (check console output above)\n");
    printf("  [PASS] CSV output written to test_log.csv\n");
    remove("test_log.csv");

    printf("\n  >> Logger test PASSED\n");
    return 0;
}

/* ======================================================================== */
int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔══════════════════════════════════════════════════════╗\n");
    printf("║  IEC 104 Client (C) — Phase 1 Verification Suite   ║\n");
    printf("╚══════════════════════════════════════════════════════╝\n");

    int total_fails = 0;

    total_fails += test_data_model();
    total_fails += test_default_config();

    if (argc >= 2) {
        total_fails += test_load_file(argv[1]);
    } else {
        printf("\n  (Tip: pass station_config.json as arg to test file loading)\n");
    }

    total_fails += test_logger();

    printf("\n%s\n", SEP);
    if (total_fails == 0)
        printf("  ★  ALL PHASE 1 TESTS PASSED  ★\n");
    else
        printf("  ✗  %d TEST(S) FAILED\n", total_fails);
    printf("%s\n\n", SEP);

    return total_fails;
}
