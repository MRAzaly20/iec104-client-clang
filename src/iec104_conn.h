/**
 * @file iec104_conn.h
 * @brief Connection Manager — lib60870 + threading
 */
#ifndef IEC104_CONN_H
#define IEC104_CONN_H

#include "iec104_types.h"

/* Forward declarations for lib60870 */
typedef struct sCS104_Connection* CS104_Connection;

/* ---- Cross-platform threading ---- */
#ifdef _WIN32
#  include <windows.h>
   typedef HANDLE              ConnThread;
   typedef CRITICAL_SECTION    ConnMutex;
#else
#  include <pthread.h>
   typedef pthread_t           ConnThread;
   typedef pthread_mutex_t     ConnMutex;
#endif

/* ---- Callback types (used for thread-safe GUI updates) ---- */
typedef void (*IEC104_PointUpdateCb)(const char *conn_id, int ioa,
                                     IEC104_Value val, IEC104_Quality q,
                                     uint64_t ts_ms, int asdu_type,
                                     void *user_data);

typedef void (*IEC104_StateChangeCb)(const char *conn_id,
                                     IEC104_ConnState state, void *user_data);

typedef void (*IEC104_LogCb)(const char *conn_id, IEC104_LogLevel level,
                             const char *msg, void *user_data);

/* ============================================================================
 * Connection context — one per RTU / server
 * ========================================================================= */
typedef struct {
    char                  id[IEC104_MAX_NAME_LEN];
    IEC104_StationConfig  config;
    IEC104_ConnState      state;

    /* lib60870 handle */
    CS104_Connection      connection;

    /* Threading */
    ConnThread            thread;
    ConnMutex             mutex;
    volatile bool         running;
    volatile bool         stop_flag;

    /* Reconnect */
    bool                  auto_reconnect;
    float                 reconnect_interval;     /* seconds */
    int                   max_reconnect_attempts; /* 0 = unlimited */
    int                   reconnect_count;

    /* Time sync */
    bool                  auto_time_sync;
    int                   time_sync_interval_sec;

    /* Callbacks */
    IEC104_PointUpdateCb  on_point_update;
    IEC104_StateChangeCb  on_state_change;
    IEC104_LogCb          on_log;
    void                 *cb_user_data;

} IEC104_Connection;

/* ---- Lifecycle ---- */
void iec104_conn_init(IEC104_Connection *c, const char *id,
                      const IEC104_StationConfig *cfg);
int  iec104_conn_start(IEC104_Connection *c);
void iec104_conn_stop(IEC104_Connection *c);
void iec104_conn_destroy(IEC104_Connection *c);

/* ---- Queries ---- */
bool iec104_conn_is_connected(const IEC104_Connection *c);

/* ---- Protocol commands ---- */
int  iec104_conn_send_gi(IEC104_Connection *c);
int  iec104_conn_send_clock_sync(IEC104_Connection *c);
int  iec104_conn_send_counter_interrogation(IEC104_Connection *c);
int  iec104_conn_send_read(IEC104_Connection *c, int ioa);
int  iec104_conn_send_test(IEC104_Connection *c);
int  iec104_conn_send_reset_process(IEC104_Connection *c, uint8_t qrp);
int  iec104_conn_send_delay_acquisition(IEC104_Connection *c, uint16_t delay_ms);

#endif /* IEC104_CONN_H */
