/**
 * @file iec104_model.h
 * @brief Data model operations for DataPoint, DataGroup, StationConfig
 */
#ifndef IEC104_MODEL_H
#define IEC104_MODEL_H

#include "iec104_types.h"

/* ---- DataPoint ---- */
void iec104_point_init(IEC104_DataPoint *pt, int ioa, const char *name,
                       IEC104_AsduType type);
void iec104_point_update(IEC104_DataPoint *pt, IEC104_Value val,
                         IEC104_Quality q, uint64_t ts_ms);

/* ---- DataGroup ---- */
void iec104_group_init(IEC104_DataGroup *grp, const char *name,
                       IEC104_AsduType type, int start_ioa,
                       int quantity, const char *desc);
IEC104_DataPoint* iec104_group_get_point(IEC104_DataGroup *grp, int ioa);
int  iec104_group_end_ioa(const IEC104_DataGroup *grp);

/* ---- StationConfig ---- */
void iec104_station_init(IEC104_StationConfig *cfg, const char *name, int ca,
                         const char *ip, int port);
int  iec104_station_add_group(IEC104_StationConfig *cfg,
                              const IEC104_DataGroup *grp);
IEC104_DataPoint* iec104_station_find_point(IEC104_StationConfig *cfg, int ioa);
IEC104_DataPoint* iec104_station_find_point_by_type(IEC104_StationConfig *cfg,
                                                     int ioa, IEC104_AsduType type);
IEC104_DataGroup* iec104_station_find_group(IEC104_StationConfig *cfg,
                                            const char *name);
int  iec104_station_total_points(const IEC104_StationConfig *cfg);
void iec104_station_print(const IEC104_StationConfig *cfg);

/* ---- Utility ---- */
bool iec104_groups_overlap(const IEC104_DataGroup *a, const IEC104_DataGroup *b);
int  iec104_type_family(int type);
bool iec104_station_config_equals(const IEC104_StationConfig *a,
                                   const IEC104_StationConfig *b);

#endif /* IEC104_MODEL_H */
