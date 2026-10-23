/**
 * @file iec104_cmd.h
 * @brief Command Sender — Direct Execute + SBO for all command types
 *
 * All command functions accept a QU (Qualifier of Command) parameter:
 *   0 = no additional definition
 *   1 = short pulse duration
 *   2 = long pulse duration
 *   3 = persistent output
 */
#ifndef IEC104_CMD_H
#define IEC104_CMD_H

#include "iec104_types.h"
#include "iec104_conn.h"

typedef enum {
    CMD_DIRECT_EXECUTE = 0,
    CMD_SELECT_BEFORE_OPERATE = 1,
} IEC104_CmdMode;

typedef enum {
    CMD_OK = 0,
    CMD_NEGATIVE,
    CMD_TIMEOUT,
    CMD_ERROR,
} IEC104_CmdResult;

/* ---- Single Command (C_SC_NA_1 / C_SC_TA_1) ---- */
IEC104_CmdResult iec104_cmd_single(IEC104_Connection *c, int ioa,
                                    bool state, bool select, int qu,
                                    bool with_time);

/* ---- Double Command (C_DC_NA_1 / C_DC_TA_1) ---- */
IEC104_CmdResult iec104_cmd_double(IEC104_Connection *c, int ioa,
                                    int dcs, bool select, int qu,
                                    bool with_time);

/* ---- Step Command (C_RC_NA_1 / C_RC_TA_1) ---- */
IEC104_CmdResult iec104_cmd_step(IEC104_Connection *c, int ioa,
                                  int step, bool select, int qu,
                                  bool with_time);

/* ---- Setpoint Normalized (C_SE_NA_1 / C_SE_TA_1) ---- */
IEC104_CmdResult iec104_cmd_setpoint_normalized(IEC104_Connection *c, int ioa,
                                                 float value, bool select,
                                                 int qu, bool with_time);

/* ---- Setpoint Scaled (C_SE_NB_1 / C_SE_TB_1) ---- */
IEC104_CmdResult iec104_cmd_setpoint_scaled(IEC104_Connection *c, int ioa,
                                             int value, bool select,
                                             int qu, bool with_time);

/* ---- Setpoint Short Float (C_SE_NC_1 / C_SE_TC_1) ---- */
IEC104_CmdResult iec104_cmd_setpoint_float(IEC104_Connection *c, int ioa,
                                            float value, bool select,
                                            int qu, bool with_time);

/* ---- Bitstring Command (C_BO_NA_1 / C_BO_TA_1) ---- */
IEC104_CmdResult iec104_cmd_bitstring(IEC104_Connection *c, int ioa,
                                       uint32_t value, bool with_time);

#endif /* IEC104_CMD_H */
