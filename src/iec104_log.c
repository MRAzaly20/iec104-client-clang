/**
 * @file iec104_log.c
 * @brief Thread-safe console + CSV logging
 */
#include "iec104_log.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

/* --- Cross-platform mutex ------------------------------------------------ */
#ifdef _WIN32
#  include <windows.h>
   typedef CRITICAL_SECTION LogMutex;
#  define MTX_INIT(m)    InitializeCriticalSection(&(m))
#  define MTX_LOCK(m)    EnterCriticalSection(&(m))
#  define MTX_UNLOCK(m)  LeaveCriticalSection(&(m))
#  define MTX_DESTROY(m) DeleteCriticalSection(&(m))
#else
#  include <pthread.h>
   typedef pthread_mutex_t LogMutex;
#  define MTX_INIT(m)    pthread_mutex_init(&(m), NULL)
#  define MTX_LOCK(m)    pthread_mutex_lock(&(m))
#  define MTX_UNLOCK(m)  pthread_mutex_unlock(&(m))
#  define MTX_DESTROY(m) pthread_mutex_destroy(&(m))
#endif

static FILE    *s_csv  = NULL;
static LogMutex s_mtx;
static bool     s_init = false;

/* ========================================================================= */

const char* iec104_log_level_name(IEC104_LogLevel level)
{
    switch (level) {
    case LOG_INFO:      return "INFO";
    case LOG_SUCCESS:   return "OK";
    case LOG_WARNING:   return "WARN";
    case LOG_ERROR:     return "ERROR";
    case LOG_COMMAND:   return "CMD";
    case LOG_RECONNECT: return "RECONN";
    default:            return "???";
    }
}

void iec104_log_init(const char *csv_path)
{
    MTX_INIT(s_mtx);
    s_init = true;

    if (csv_path && csv_path[0]) {
        s_csv = fopen(csv_path, "w");
        if (s_csv) {
            fprintf(s_csv, "timestamp,level,message\n");
            fflush(s_csv);
        }
    }
}

void iec104_log_shutdown(void)
{
    if (!s_init) return;
    MTX_LOCK(s_mtx);
    if (s_csv) { fclose(s_csv); s_csv = NULL; }
    MTX_UNLOCK(s_mtx);
    MTX_DESTROY(s_mtx);
    s_init = false;
}

void iec104_log(IEC104_LogLevel level, const char *fmt, ...)
{
    char msg[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);

    /* Timestamp */
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char ts[32];
    strftime(ts, sizeof(ts), "%H:%M:%S", t);

    if (s_init) MTX_LOCK(s_mtx);

    printf("[%s][%-6s] %s\n", ts, iec104_log_level_name(level), msg);
    fflush(stdout);

    if (s_csv) {
        fprintf(s_csv, "%s,%s,\"%s\"\n", ts, iec104_log_level_name(level), msg);
        fflush(s_csv);
    }

    if (s_init) MTX_UNLOCK(s_mtx);
}

void iec104_log_traffic(const IEC104_TrafficEntry *entry)
{
    iec104_log(LOG_INFO, "[%s] %-12s IOA=%-5d %s",
               entry->is_tx ? "TX" : "RX",
               entry->type_name,
               entry->ioa,
               entry->details);
}
