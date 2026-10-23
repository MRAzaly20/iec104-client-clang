/**
 * @file iec104_types.h
 * @brief Shared type definitions for IEC 104 Client
 *
 * Central header with all enums, structs, and constants used across modules.
 * Every module includes this header for common data types.
 */
#ifndef IEC104_TYPES_H
#define IEC104_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * Compile-‌time limits
 * ========================================================================= */
#define IEC104_MAX_NAME_LEN       64   /* group / station / point name */
#define IEC104_MAX_DESC_LEN       128  /* descriptions */
#define IEC104_MAX_IP_LEN         46   /* IPv4 / IPv6 string */
#define IEC104_MAX_GROUPS         32   /* groups per station */
#define IEC104_MAX_POINTS_PER_GRP 256  /* points per group */
#define IEC104_MAX_CONNECTIONS    8    /* simultaneous connections */
#define IEC104_MAX_LOG_ENTRIES    2000 /* circular-log depth */

/* ============================================================================
 * ASDU Type Enum
 *
 * Mirrors the IEC 60870-5 Type Identification numbers.
 * Only the types we actually use are listed; the numeric values
 * intentionally match the standard so we can cast to/from lib60870 TypeID.
 * ========================================================================= */
typedef enum {
    /* ---- Monitoring (Process Information) ---- */
    ASDU_M_SP_NA_1 =  1,   /* Single-point information               */
    ASDU_M_SP_TA_1 =  2,   /* Single-point with time tag (CP24)      */
    ASDU_M_DP_NA_1 =  3,   /* Double-point information               */
    ASDU_M_DP_TA_1 =  4,   /* Double-point with time tag (CP24)      */
    ASDU_M_ST_NA_1 =  5,   /* Step position information              */
    ASDU_M_ST_TA_1 =  6,   /* Step position with time tag (CP24)     */
    ASDU_M_BO_NA_1 =  7,   /* Bitstring of 32 bits                   */
    ASDU_M_BO_TA_1 =  8,   /* Bitstring with time tag (CP24)         */
    ASDU_M_ME_NA_1 =  9,   /* Measured value, normalized             */
    ASDU_M_ME_TA_1 = 10,   /* Measured normalized + CP24Time2a       */
    ASDU_M_ME_NB_1 = 11,   /* Measured value, scaled                 */
    ASDU_M_ME_TB_1 = 12,   /* Measured scaled + CP24Time2a           */
    ASDU_M_ME_NC_1 = 13,   /* Measured value, short float            */
    ASDU_M_ME_TC_1 = 14,   /* Measured short float + CP24Time2a      */
    ASDU_M_IT_NA_1 = 15,   /* Integrated totals                      */
    ASDU_M_IT_TA_1 = 16,   /* Integrated totals + CP24Time2a         */
    ASDU_M_EP_TA_1 = 17,   /* Protection event + CP24Time2a          */
    ASDU_M_EP_TB_1 = 18,   /* Protection start events + CP24Time2a   */
    ASDU_M_EP_TC_1 = 19,   /* Protection output circuit + CP24Time2a */
    ASDU_M_PS_NA_1 = 20,   /* Packed single-point with SCD           */
    ASDU_M_ME_ND_1 = 21,   /* Normalized without quality descriptor  */
    ASDU_M_SP_TB_1 = 30,   /* Single-point + CP56Time2a              */
    ASDU_M_DP_TB_1 = 31,   /* Double-point + CP56Time2a              */
    ASDU_M_ST_TB_1 = 32,   /* Step position + CP56Time2a             */
    ASDU_M_BO_TB_1 = 33,   /* Bitstring  + CP56Time2a                */
    ASDU_M_ME_TD_1 = 34,   /* Normalized + CP56Time2a                */
    ASDU_M_ME_TE_1 = 35,   /* Scaled     + CP56Time2a                */
    ASDU_M_ME_TF_1 = 36,   /* Short float + CP56Time2a               */
    ASDU_M_IT_TB_1 = 37,   /* Integrated totals + CP56Time2a         */
    ASDU_M_EP_TD_1 = 38,   /* Protection event + CP56Time2a          */
    ASDU_M_EP_TE_1 = 39,   /* Protection start events + CP56Time2a   */
    ASDU_M_EP_TF_1 = 40,   /* Protection output circuit + CP56Time2a */
    ASDU_M_EI_NA_1 = 70,   /* End of initialization                  */

    /* ---- Control (Commands) ---- */
    ASDU_C_SC_NA_1 = 45,   /* Single command                         */
    ASDU_C_DC_NA_1 = 46,   /* Double command                         */
    ASDU_C_RC_NA_1 = 47,   /* Regulating step command                */
    ASDU_C_SE_NA_1 = 48,   /* Set-point, normalized                  */
    ASDU_C_SE_NB_1 = 49,   /* Set-point, scaled                      */
    ASDU_C_SE_NC_1 = 50,   /* Set-point, short float                 */
    ASDU_C_BO_NA_1 = 51,   /* Bitstring of 32-bit command            */
    ASDU_C_SC_TA_1 = 58,   /* Single command      + CP56Time2a       */
    ASDU_C_DC_TA_1 = 59,   /* Double command      + CP56Time2a       */
    ASDU_C_RC_TA_1 = 60,   /* Step command         + CP56Time2a      */
    ASDU_C_SE_TA_1 = 61,   /* Set-point normalized + CP56Time2a      */
    ASDU_C_SE_TB_1 = 62,   /* Set-point scaled     + CP56Time2a      */
    ASDU_C_SE_TC_1 = 63,   /* Set-point short float + CP56Time2a     */
    ASDU_C_BO_TA_1 = 64,   /* Bitstring command    + CP56Time2a      */

    /* ---- System / Control in Monitor Direction ---- */
    ASDU_C_IC_NA_1 = 100,  /* Interrogation command                  */
    ASDU_C_CI_NA_1 = 101,  /* Counter interrogation                  */
    ASDU_C_RD_NA_1 = 102,  /* Read command                           */
    ASDU_C_CS_NA_1 = 103,  /* Clock synchronization                  */
    ASDU_C_TS_NA_1 = 104,  /* Test command                           */
    ASDU_C_RP_NA_1 = 105,  /* Reset process command                  */
    ASDU_C_CD_NA_1 = 106,  /* Delay acquisition command              */
    ASDU_C_TS_TA_1 = 107,  /* Test command with CP56Time2a           */
} IEC104_AsduType;

/* ============================================================================
 * Data Point Value — tagged union
 *
 * IEC 104 points carry different payload types; this struct stores the
 * current value together with a discriminator so consumers can switch on it.
 * ========================================================================= */
typedef enum {
    VAL_NONE = 0,
    VAL_BOOL,           /* single-point on/off                    */
    VAL_INT32,          /* scaled value / step position           */
    VAL_FLOAT,          /* short float / normalized               */
    VAL_UINT32,         /* bitstring 32                           */
    VAL_DOUBLE_POINT,   /* 0=INDET, 1=OFF, 2=ON, 3=INDET         */
} IEC104_ValueType;

typedef struct {
    IEC104_ValueType type;
    union {
        bool        b;
        int32_t     i;
        float       f;
        uint32_t    bits;
    } v;                /* named union member for C99 compat */
} IEC104_Value;

/* ============================================================================
 * Quality Flags
 * ========================================================================= */
typedef enum {
    QUALITY_GOOD    = 0,
    QUALITY_BAD     = 1,
    QUALITY_INVALID = 2,
    QUALITY_UNKNOWN = 3,
} IEC104_Quality;

/* ============================================================================
 * Data Point
 * ========================================================================= */
typedef struct {
    int             ioa;                        /* Information Object Address  */
    char            name[IEC104_MAX_NAME_LEN];  /* human-readable name        */
    IEC104_AsduType asdu_type;                  /* ASDU type this point belongs to */
    IEC104_Value    value;                      /* current value              */
    IEC104_Quality  quality;                    /* quality flags              */
    uint64_t        timestamp_ms;               /* UTC ms since epoch        */
    bool            updated;                    /* dirty flag for GUI refresh */
} IEC104_DataPoint;

/* ============================================================================
 * Data Group — a range of consecutive IOAs sharing the same ASDU type.
 * Points are auto-generated from start_ioa + quantity in iec104_group_init().
 * ========================================================================= */
typedef struct {
    char            group_name[IEC104_MAX_NAME_LEN];
    IEC104_AsduType asdu_type;
    int             start_ioa;
    int             quantity;
    char            description[IEC104_MAX_DESC_LEN];

    IEC104_DataPoint points[IEC104_MAX_POINTS_PER_GRP];
    int             point_count;    /* equals min(quantity, MAX_POINTS_PER_GRP) */
} IEC104_DataGroup;

/* ============================================================================
 * Station Configuration — all information needed to connect and poll one RTU.
 * ========================================================================= */
typedef struct {
    char            name[IEC104_MAX_NAME_LEN];
    int             common_address;     /* 1–65534 (CASDU / COA) */
    char            server_ip[IEC104_MAX_IP_LEN];
    int             server_port;        /* default 2404 */

    IEC104_DataGroup groups[IEC104_MAX_GROUPS];
    int             group_count;
} IEC104_StationConfig;

/* ============================================================================
 * Connection State
 * ========================================================================= */
typedef enum {
    CONN_DISCONNECTED = 0,
    CONN_CONNECTING,
    CONN_CONNECTED,
    CONN_RECONNECTING,
    CONN_ERROR,
    CONN_TIMEOUT,
} IEC104_ConnState;

/* ============================================================================
 * Log Level
 * ========================================================================= */
typedef enum {
    LOG_INFO = 0,
    LOG_SUCCESS,
    LOG_WARNING,
    LOG_ERROR,
    LOG_COMMAND,
    LOG_RECONNECT,
} IEC104_LogLevel;

/* ============================================================================
 * Log Entry
 * ========================================================================= */
typedef struct {
    uint64_t        timestamp_ms;
    IEC104_LogLevel level;
    char            message[256];
} IEC104_LogEntry;

/* ============================================================================
 * Traffic Entry (for protocol monitor)
 * ========================================================================= */
typedef struct {
    uint64_t    timestamp_ms;
    bool        is_tx;          /* true = sent, false = received */
    char        type_name[32];
    int         ioa;
    char        details[128];
    char        category[16];   /* "MON", "CMD", "SYS" */
} IEC104_TrafficEntry;

/* ============================================================================
 * Utility functions declared here, implemented in iec104_model.c
 * ========================================================================= */

/** Return human-readable type string, e.g. "M_SP_NA_1". */
const char* iec104_asdu_type_name(IEC104_AsduType type);

/** True for monitoring types (M_*). */
bool iec104_is_monitoring_type(IEC104_AsduType type);

/** True for command/system types (C_*). */
bool iec104_is_command_type(IEC104_AsduType type);

/** Return connection state as a string. */
const char* iec104_conn_state_name(IEC104_ConnState state);

#endif /* IEC104_TYPES_H */
