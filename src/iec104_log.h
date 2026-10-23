/**
 * @file iec104_log.h
 * @brief Thread-safe logging — console + CSV
 */
#ifndef IEC104_LOG_H
#define IEC104_LOG_H

#include "iec104_types.h"

/** Initialise logger. @p csv_path may be NULL to disable CSV output. */
void iec104_log_init(const char *csv_path);

/** Shut down logger and close CSV file. */
void iec104_log_shutdown(void);

/** Log a formatted message. Thread-safe. */
void iec104_log(IEC104_LogLevel level, const char *fmt, ...);

/** Log a structured traffic entry. Thread-safe. */
void iec104_log_traffic(const IEC104_TrafficEntry *entry);

/** Return log-level name, e.g. "INFO". */
const char* iec104_log_level_name(IEC104_LogLevel level);

#endif /* IEC104_LOG_H */
