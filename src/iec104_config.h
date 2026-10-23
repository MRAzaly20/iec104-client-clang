/**
 * @file iec104_config.h
 * @brief JSON configuration load/save via cJSON
 */
#ifndef IEC104_CONFIG_H
#define IEC104_CONFIG_H

#include "iec104_types.h"

/**
 * Load station configs from a JSON file.
 * Supports both multi-connection {"connections":[...]} and
 * old single-station bare-object format.
 *
 * @param filepath     Path to the JSON config file.
 * @param configs      Output array (caller-allocated).
 * @param max_configs  Maximum entries to load.
 * @return Number of configs loaded (>= 0), or -1 on error.
 */
int iec104_config_load(const char *filepath,
                       IEC104_StationConfig *configs, int max_configs);

/**
 * Save station configs to a JSON file (multi-connection format).
 *
 * @return 0 on success, -1 on error.
 */
int iec104_config_save(const char *filepath,
                       const IEC104_StationConfig *configs, int count);


/** Parse a type-name string ("M_SP_NA_1") to its enum value.
 *  Returns 0 on failure. */
IEC104_AsduType iec104_asdu_type_from_string(const char *str);

#endif /* IEC104_CONFIG_H */
