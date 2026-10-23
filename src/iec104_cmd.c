/**
 * @file iec104_cmd.c
 * @brief Command Sender — all 7 command families, Direct + SBO
 *
 * QU qualifier is passed through to lib60870 create functions.
 * Bitstring now supports time-tagged variant (C_BO_TA_1).
 */
#include "iec104_cmd.h"
#include "iec104_log.h"

#include "cs104_connection.h"
#include "cs101_information_objects.h"
#include "hal_time.h"

#include <stdio.h>
#include <string.h>

/* ---- helpers ---- */
static struct sCP56Time2a now_ts(void)
{
    struct sCP56Time2a ts;
    CP56Time2a_createFromMsTimestamp(&ts, Hal_getTimeInMs());
    return ts;
}

static IEC104_CmdResult wrap(bool ok) { return ok ? CMD_OK : CMD_ERROR; }

/* ============================================================================
 * Single Command  (C_SC_NA_1 / C_SC_TA_1)
 * ========================================================================= */
IEC104_CmdResult iec104_cmd_single(IEC104_Connection *c, int ioa,
                                    bool state, bool select, int qu,
                                    bool with_time)
{
    if (!c->connection || c->state != CONN_CONNECTED) return CMD_ERROR;
    bool ok;

    if (with_time) {
        struct sCP56Time2a ts = now_ts();
        SingleCommandWithCP56Time2a cmd =
            SingleCommandWithCP56Time2a_create(NULL, ioa, state, select, qu, &ts);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SingleCommandWithCP56Time2a_destroy(cmd);
    } else {
        SingleCommand cmd = SingleCommand_create(NULL, ioa, state, select, qu);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SingleCommand_destroy(cmd);
    }
    iec104_log(LOG_COMMAND, "SC%s IOA=%d state=%d sel=%d qu=%d -> %s",
               with_time ? "_TA" : "", ioa, state, select, qu, ok ? "OK" : "FAIL");
    return wrap(ok);
}

/* ============================================================================
 * Double Command  (C_DC_NA_1 / C_DC_TA_1)
 * ========================================================================= */
IEC104_CmdResult iec104_cmd_double(IEC104_Connection *c, int ioa,
                                    int dcs, bool select, int qu,
                                    bool with_time)
{
    if (!c->connection || c->state != CONN_CONNECTED) return CMD_ERROR;
    bool ok;

    if (with_time) {
        struct sCP56Time2a ts = now_ts();
        DoubleCommandWithCP56Time2a cmd =
            DoubleCommandWithCP56Time2a_create(NULL, ioa, dcs, select, qu, &ts);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        DoubleCommandWithCP56Time2a_destroy(cmd);
    } else {
        DoubleCommand cmd = DoubleCommand_create(NULL, ioa, dcs, select, qu);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        DoubleCommand_destroy(cmd);
    }
    iec104_log(LOG_COMMAND, "DC%s IOA=%d dcs=%d sel=%d qu=%d -> %s",
               with_time ? "_TA" : "", ioa, dcs, select, qu, ok ? "OK" : "FAIL");
    return wrap(ok);
}

/* ============================================================================
 * Step Command  (C_RC_NA_1 / C_RC_TA_1)
 * ========================================================================= */
IEC104_CmdResult iec104_cmd_step(IEC104_Connection *c, int ioa,
                                  int step, bool select, int qu,
                                  bool with_time)
{
    if (!c->connection || c->state != CONN_CONNECTED) return CMD_ERROR;
    bool ok;

    if (with_time) {
        struct sCP56Time2a ts = now_ts();
        StepCommandWithCP56Time2a cmd =
            StepCommandWithCP56Time2a_create(NULL, ioa, step, select, qu, &ts);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        StepCommandWithCP56Time2a_destroy(cmd);
    } else {
        StepCommand cmd = StepCommand_create(NULL, ioa, step, select, qu);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        StepCommand_destroy(cmd);
    }
    iec104_log(LOG_COMMAND, "RC%s IOA=%d step=%d sel=%d qu=%d -> %s",
               with_time ? "_TA" : "", ioa, step, select, qu, ok ? "OK" : "FAIL");
    return wrap(ok);
}

/* ============================================================================
 * Setpoint Normalized  (C_SE_NA_1 / C_SE_TA_1)
 * ========================================================================= */
IEC104_CmdResult iec104_cmd_setpoint_normalized(IEC104_Connection *c, int ioa,
                                                 float value, bool select,
                                                 int qu, bool with_time)
{
    if (!c->connection || c->state != CONN_CONNECTED) return CMD_ERROR;
    bool ok;

    if (with_time) {
        struct sCP56Time2a ts = now_ts();
        SetpointCommandNormalizedWithCP56Time2a cmd =
            SetpointCommandNormalizedWithCP56Time2a_create(NULL, ioa, value,
                                                           select, qu, &ts);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SetpointCommandNormalizedWithCP56Time2a_destroy(cmd);
    } else {
        SetpointCommandNormalized cmd =
            SetpointCommandNormalized_create(NULL, ioa, value, select, qu);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SetpointCommandNormalized_destroy(cmd);
    }
    iec104_log(LOG_COMMAND, "SE_NA%s IOA=%d val=%.4f sel=%d qu=%d -> %s",
               with_time ? "_TA" : "", ioa, value, select, qu, ok ? "OK" : "FAIL");
    return wrap(ok);
}

/* ============================================================================
 * Setpoint Scaled  (C_SE_NB_1 / C_SE_TB_1)
 * ========================================================================= */
IEC104_CmdResult iec104_cmd_setpoint_scaled(IEC104_Connection *c, int ioa,
                                             int value, bool select,
                                             int qu, bool with_time)
{
    if (!c->connection || c->state != CONN_CONNECTED) return CMD_ERROR;
    bool ok;

    if (with_time) {
        struct sCP56Time2a ts = now_ts();
        SetpointCommandScaledWithCP56Time2a cmd =
            SetpointCommandScaledWithCP56Time2a_create(NULL, ioa, value,
                                                       select, qu, &ts);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SetpointCommandScaledWithCP56Time2a_destroy(cmd);
    } else {
        SetpointCommandScaled cmd =
            SetpointCommandScaled_create(NULL, ioa, value, select, qu);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SetpointCommandScaled_destroy(cmd);
    }
    iec104_log(LOG_COMMAND, "SE_NB%s IOA=%d val=%d sel=%d qu=%d -> %s",
               with_time ? "_TA" : "", ioa, value, select, qu, ok ? "OK" : "FAIL");
    return wrap(ok);
}

/* ============================================================================
 * Setpoint Short Float  (C_SE_NC_1 / C_SE_TC_1)
 * ========================================================================= */
IEC104_CmdResult iec104_cmd_setpoint_float(IEC104_Connection *c, int ioa,
                                            float value, bool select,
                                            int qu, bool with_time)
{
    if (!c->connection || c->state != CONN_CONNECTED) return CMD_ERROR;
    bool ok;

    if (with_time) {
        struct sCP56Time2a ts = now_ts();
        SetpointCommandShortWithCP56Time2a cmd =
            SetpointCommandShortWithCP56Time2a_create(NULL, ioa, value,
                                                      select, qu, &ts);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SetpointCommandShortWithCP56Time2a_destroy(cmd);
    } else {
        SetpointCommandShort cmd =
            SetpointCommandShort_create(NULL, ioa, value, select, qu);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        SetpointCommandShort_destroy(cmd);
    }
    iec104_log(LOG_COMMAND, "SE_NC%s IOA=%d val=%.4f sel=%d qu=%d -> %s",
               with_time ? "_TA" : "", ioa, value, select, qu, ok ? "OK" : "FAIL");
    return wrap(ok);
}

/* ============================================================================
 * Bitstring Command  (C_BO_NA_1 / C_BO_TA_1)
 * ========================================================================= */
IEC104_CmdResult iec104_cmd_bitstring(IEC104_Connection *c, int ioa,
                                       uint32_t value, bool with_time)
{
    if (!c->connection || c->state != CONN_CONNECTED) return CMD_ERROR;
    bool ok;

    if (with_time) {
        struct sCP56Time2a ts = now_ts();
        Bitstring32CommandWithCP56Time2a cmd =
            Bitstring32CommandWithCP56Time2a_create(NULL, ioa, value, &ts);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        Bitstring32CommandWithCP56Time2a_destroy(cmd);
    } else {
        Bitstring32Command cmd = Bitstring32Command_create(NULL, ioa, value);
        ok = CS104_Connection_sendProcessCommandEx(c->connection,
            CS101_COT_ACTIVATION, c->config.common_address, (InformationObject)cmd);
        Bitstring32Command_destroy(cmd);
    }

    iec104_log(LOG_COMMAND, "BO%s IOA=%d val=0x%08X -> %s",
               with_time ? "_TA" : "", ioa, value, ok ? "OK" : "FAIL");
    return wrap(ok);
}
