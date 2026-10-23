# IEC 104 Client: Python → C Migration

## Phase 1: Foundation

- [X] CMakeLists.txt (lib60870 + cJSON + GTK4 + pthreads)
- [X] `iec104_types.h` — shared enums/structs
- [ ] `iec104_model.c` — DataPoint, DataGroup, StationConfig
- [ ] `iec104_config.c` — JSON load/save via cJSON
- [ ] Verify: load station_config.json, print config

## Phase 2: Connection + Protocol

- [ ] `iec104_conn.c` — connect, ASDU dispatcher, auto-reconnect
- [ ] ASDU received handler (all M_ types)
- [ ] Periodic time sync
- [ ] `iec104_cmd.c` — command types, Direct + SBO
- [ ] `iec104_log.c` — console + CSV
- [ ] Console-only test: connect → GI → receive → send commands

## Phase 3: GTK4 UI — Main Window

- [ ] `main.c` + GtkApplication
- [ ] Dark SCADA CSS theme
- [ ] Header bar + toolbar buttons
- [ ] Sidebar connection tree
- [ ] Data grid (GtkColumnView)
- [ ] Status bar

## Phase 4: GTK4 UI — Dialogs + Commands

- [ ] Command control panel (SC/DC/RC/SE)
- [ ] Communication log panel
- [ ] Traffic monitor
- [ ] Edit Slave dialog
- [ ] New Connection dialog

## Phase 5: Integration + Polish

- [ ] Wire GTK4 ↔ connection manager
- [ ] Multi-connection support
- [ ] Config save/reload
- [ ] CSV export
- [ ] End-to-end testing
