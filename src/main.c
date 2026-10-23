/**
 * @file main.c
 * @brief IEC 104 Client — Console test (Phase 2 verification)
 *
 * Connects to simple_server (COA=47), receives GI data + spontaneous events,
 * then sends all 14 command types. Tests auto-reconnect on server restart.
 *
 * Usage: iec104_client_console [config.json] [server_ip] [port]
 */
#include "iec104_types.h"
#include "iec104_model.h"
#include "iec104_config.h"
#include "iec104_conn.h"
#include "iec104_cmd.h"
#include "iec104_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#ifdef _WIN32
#  include <windows.h>
#  define SLEEP_MS(ms) Sleep(ms)
#else
#  include <unistd.h>
#  define SLEEP_MS(ms) usleep((ms) * 1000)
#endif

static volatile bool g_running = true;

#ifdef _WIN32
static BOOL WINAPI consoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT) { g_running = false; return TRUE; }
    return FALSE;
}
#else
static void signal_handler(int sig) { (void)sig; g_running = false; }
#endif

/* ============================================================================
 * Callbacks — print received data to console
 * ========================================================================= */
static void on_point_update(const char *conn_id, int ioa,
                            IEC104_Value val, IEC104_Quality q,
                            uint64_t ts_ms, int asdu_type, void *user_data)
{
    (void)user_data; (void)ts_ms; (void)asdu_type;
    const char *qs = (q == QUALITY_GOOD) ? "GOOD" : "BAD";

    switch (val.type) {
    case VAL_BOOL:
        printf("  [%s] IOA=%-4d  bool=%-5s  q=%s\n",
               conn_id, ioa, val.v.b ? "ON" : "OFF", qs);
        break;
    case VAL_DOUBLE_POINT:
        printf("  [%s] IOA=%-4d  dp=%-7d  q=%s\n",
               conn_id, ioa, val.v.i, qs);
        break;
    case VAL_INT32:
        printf("  [%s] IOA=%-4d  int=%-6d  q=%s\n",
               conn_id, ioa, val.v.i, qs);
        break;
    case VAL_FLOAT:
        printf("  [%s] IOA=%-4d  float=%.4f  q=%s\n",
               conn_id, ioa, val.v.f, qs);
        break;
    case VAL_UINT32:
        printf("  [%s] IOA=%-4d  bits=0x%08X  q=%s\n",
               conn_id, ioa, val.v.bits, qs);
        break;
    default:
        printf("  [%s] IOA=%-4d  (none)  q=%s\n",
               conn_id, ioa, qs);
        break;
    }
}

static void on_state_change(const char *conn_id, IEC104_ConnState state,
                            void *user_data)
{
    (void)user_data;
    printf("\n*** [%s] --> %s ***\n\n", conn_id, iec104_conn_state_name(state));
}

/* ============================================================================
 * Command test sequence — exercises all 14 command types
 * IOAs chosen to match simple_server (accepts any IOA)
 * ========================================================================= */
static void run_command_tests(IEC104_Connection *c)
{
    printf("\n========================================\n");
    printf("  COMMAND TEST SEQUENCE\n");
    printf("========================================\n\n");

    IEC104_CmdResult r;

    /* ---- Commands without time tag (NA) ---- */

    /* C_SC_NA_1: Single Command ON */
    printf("--- C_SC_NA_1  Single ON  IOA=4 ---\n");
    r = iec104_cmd_single(c, 4, true, false, 0, false);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_SC_NA_1: Single Command OFF */
    printf("--- C_SC_NA_1  Single OFF IOA=5 ---\n");
    r = iec104_cmd_single(c, 5, false, false, 0, false);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_DC_NA_1: Double Command ON */
    printf("--- C_DC_NA_1  Double ON  IOA=6 ---\n");
    r = iec104_cmd_double(c, 6, 2, false, 0, false);  /* 2=ON */
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_DC_NA_1: Double Command OFF */
    printf("--- C_DC_NA_1  Double OFF IOA=8 ---\n");
    r = iec104_cmd_double(c, 8, 1, false, 0, false);  /* 1=OFF */
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_RC_NA_1: Step HIGHER */
    printf("--- C_RC_NA_1  Step HIGHER IOA=7 ---\n");
    r = iec104_cmd_step(c, 7, 2, false, 0, false);  /* 2=HIGHER */
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_SE_NA_1: Setpoint Normalized */
    printf("--- C_SE_NA_1  Norm=0.75  IOA=9 ---\n");
    r = iec104_cmd_setpoint_normalized(c, 9, 0.75f, false, 0, false);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_SE_NB_1: Setpoint Scaled */
    printf("--- C_SE_NB_1  Scaled=1234  IOA=10 ---\n");
    r = iec104_cmd_setpoint_scaled(c, 10, 1234, false, 0, false);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_SE_NC_1: Setpoint Float */
    printf("--- C_SE_NC_1  Float=3.14  IOA=11 ---\n");
    r = iec104_cmd_setpoint_float(c, 11, 3.14f, false, 0, false);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_BO_NA_1: Bitstring */
    printf("--- C_BO_NA_1  Bits=0xDEADBEEF  IOA=12 ---\n");
    r = iec104_cmd_bitstring(c, 12, 0xDEADBEEF, false);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* ---- Commands with time tag (TA) ---- */

    /* C_SC_TA_1: Single Command + time */
    printf("--- C_SC_TA_1  Single ON+time  IOA=13 ---\n");
    r = iec104_cmd_single(c, 13, true, false, 0, true);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_DC_TA_1: Double Command + time */
    printf("--- C_DC_TA_1  Double ON+time  IOA=14 ---\n");
    r = iec104_cmd_double(c, 14, 2, false, 0, true);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_RC_TA_1: Step + time */
    printf("--- C_RC_TA_1  Step LOWER+time  IOA=15 ---\n");
    r = iec104_cmd_step(c, 15, 1, false, 0, true);  /* 1=LOWER */
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* C_SE_TC_1: Setpoint Float + time */
    printf("--- C_SE_TC_1  Float=99.99+time  IOA=18 ---\n");
    r = iec104_cmd_setpoint_float(c, 18, 99.99f, false, 0, true);
    printf("    Result: %s\n\n", r == CMD_OK ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* ---- System Commands ---- */

    /* Counter Interrogation */
    printf("--- C_CI_NA_1  Counter Interrogation ---\n");
    int rc = iec104_conn_send_counter_interrogation(c);
    printf("    Result: %s\n\n", rc == 0 ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* Clock Sync */
    printf("--- C_CS_NA_1  Clock Sync ---\n");
    rc = iec104_conn_send_clock_sync(c);
    printf("    Result: %s\n\n", rc == 0 ? "OK" : "FAIL");
    SLEEP_MS(300);

    /* Read Command */
    printf("--- C_RD_NA_1  Read IOA=5 ---\n");
    rc = iec104_conn_send_read(c, 5);
    printf("    Result: %s\n\n", rc == 0 ? "OK" : "FAIL");
    SLEEP_MS(300);

    printf("========================================\n");
    printf("  ALL COMMANDS SENT!\n");
    printf("========================================\n");
}

/* ============================================================================
 * Main
 * ========================================================================= */
int main(int argc, char *argv[])
{
    /* MSYS2/mintty pipes stdout → full buffering. Force unbuffered. */
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    printf("\n");
    printf("=== IEC 104 Client (C) — Console Mode ===\n");
    printf("=== Phase 2: Connection + Protocol     ===\n\n");

#ifdef _WIN32
    SetConsoleCtrlHandler(consoleHandler, TRUE);
#else
    signal(SIGINT, signal_handler);
#endif

    iec104_log_init("iec104_traffic.csv");

    /* Large structs — MUST be static, not on stack!
     * IEC104_StationConfig is ~775 KB each (32 groups × 256 points).
     * Putting configs[8] + IEC104_Connection on the 1 MB Windows stack
     * causes an instant, silent stack overflow. */
    static IEC104_StationConfig configs[IEC104_MAX_CONNECTIONS];
    static IEC104_Connection conn;
    int num_configs = 0;

    if (argc >= 2) {
        printf("Loading config: %s\n", argv[1]);
        num_configs = iec104_config_load(argv[1], configs, IEC104_MAX_CONNECTIONS);
    }

    if (num_configs <= 0) {
        fprintf(stderr, "ERROR: No valid configuration loaded.\n");
        fprintf(stderr, "Usage: %s <station_config.json> [ip] [port]\n", argv[0]);
        return 1;
    }

    /* CLI overrides */
    if (argc >= 3)
        snprintf(configs[0].server_ip, sizeof(configs[0].server_ip), "%s", argv[2]);
    if (argc >= 4)
        configs[0].server_port = atoi(argv[3]);

    /* Print config summary */
    printf("\n========================================\n");
    for (int c = 0; c < num_configs; c++)
        iec104_station_print(&configs[c]);
    printf("========================================\n\n");

    /* ---- Start connection ---- */
    iec104_conn_init(&conn, configs[0].name, &configs[0]);
    conn.on_point_update  = on_point_update;
    conn.on_state_change  = on_state_change;
    conn.auto_time_sync   = true;
    conn.time_sync_interval_sec = 3600;

    printf("Starting connection: %s --> %s:%d (COA=%d)\n",
           conn.id, configs[0].server_ip, configs[0].server_port,
           configs[0].common_address);
    iec104_conn_start(&conn);

    printf("Press Ctrl+C to quit.\n\n");

    /* ---- Wait for connection (up to 10s) ---- */
    for (int i = 0; i < 100 && g_running; i++) {
        SLEEP_MS(100);
        if (conn.state == CONN_CONNECTED) break;
    }

    if (conn.state == CONN_CONNECTED && g_running) {
        /* Wait for GI to complete (server sends 9 stages) */
        printf("Waiting for GI response...\n");
        SLEEP_MS(3000);

        /* Wait for some spontaneous events */
        printf("\nWaiting for spontaneous events (6s)...\n");
        SLEEP_MS(6000);

        /* Run full command test sequence */
        if (g_running)
            run_command_tests(&conn);

        /* Monitor periodic data + spontaneous events */
        printf("\nMonitoring live data... (Ctrl+C to quit)\n\n");
    } else if (g_running) {
        printf("Connection failed. Will auto-reconnect...\n");
    }

    /* ---- Keep running until Ctrl+C ---- */
    while (g_running)
        SLEEP_MS(500);

    /* ---- Shutdown ---- */
    printf("\nShutting down...\n");
    iec104_conn_destroy(&conn);
    iec104_log_shutdown();
    printf("Done.\n");
    return 0;
}
