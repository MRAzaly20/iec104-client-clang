# IEC 104 Client (C)

## Project Overview

A C-language **IEC 60870-5-104 (IEC 104)** client application for industrial IoT (IIoT) gateway device management. IEC 104 is a telecontrol protocol widely used in power systems, SCADA, and energy management. This project provides both a **console client** and a **GTK4 GUI client** for communicating with IEC 104 servers/RTUs.

### Architecture

The codebase is organized into three library layers:

| Library | Purpose |
|---------|---------|
| `iec104_core` | Shared core — data model, config parsing, logging, connection management, command dispatch |
| `cjson` | JSON parsing (single-file cJSON library) |
| `lib60870` | Pre-built static library from the lib60870-C project (IEC 60870-5 protocol stack) |

Two executables link against `iec104_core`:

| Executable | Description |
|------------|-------------|
| `iec104_client_console` | Console-mode client — connects, receives GI (general interrogation), monitors spontaneous events, sends all 14+ command types |
| `iec104_client_gui` | GTK4 GUI client — dark SCADA theme, data grid, command control panel, traffic monitor, multi-connection support |

### Source Modules

| File | Responsibility |
|------|----------------|
| `iec104_types.h` | Central type definitions — ASDU types, data points, quality flags, connection states, log entries |
| `iec104_model.h/c` | DataPoint, DataGroup, StationConfig — model operations and validation |
| `iec104_config.h/c` | JSON config load/save via cJSON |
| `iec104_conn.h/c` | Connection manager — lib60870 wrapper, threading, auto-reconnect, time sync, protocol commands |
| `iec104_cmd.h/c` | Command dispatch — single/double/step/setpoint/bitstring commands (with/without time tags, QU qualifier support) |
| `iec104_log.h/c` | Logging — console output + CSV traffic export |
| `iec104_gui.h/c` | GTK4 UI — main window, data grid, command control panel, dialogs |
| `main.c` | Console client entry point (Phase 2) |
| `main_gui.c` | GUI client entry point (Phase 3+) |
| `test_phase1.c` | Phase 1 foundation test |

### Key Dependencies

- **lib60870-C** — IEC 60870-5 protocol stack (located at `../lib60870/lib60870-C/`)
- **cJSON** — JSON parsing (bundled in `lib/cJSON/`)
- **GTK4** — GUI framework (discovered via pkg-config)
- **C11** standard, Windows (ws2_32) / POSIX (pthread, m) platform support

## Building and Running

### Prerequisites

- **CMake** 3.16+
- **GCC / MinGW** (MSYS2 recommended on Windows)
- **GTK4** development packages
- **lib60870-C** built and placed at `../lib60870/lib60870-C/`

### Build Commands

```bash
# From project root
cmake --build .
```

Or if configuring from scratch:

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Running

```bash
# Console client (default config)
iec104_client_console.exe

# Console client with custom config / server
iec104_client_console.exe station_config.json 127.0.0.1 2404

# GUI client
iec104_client_gui.exe
```

### Configuration

Connection settings are defined in `station_config.json`. Each connection entry specifies:

- `name` — human-readable identifier
- `common_address` — CASDU / COA (1–65534)
- `server_ip` / `server_port` — IEC 104 server endpoint (default port 2404)
- `groups` — data point groups with ASDU type, IOA range, and quantity

CLI arguments override the config file for quick testing.

## Development Phases

The project follows a phased implementation plan (see `docs/task.md`):

| Phase | Status | Scope |
|-------|--------|-------|
| **Phase 1: Foundation** | ✅ Complete | CMakeLists, types, model, config, JSON loading |
| **Phase 2: Connection + Protocol** | ✅ Complete | Connection mgmt, ASDU dispatcher, auto-reconnect, commands, logging, console test |
| **Phase 3: GTK4 UI — Main Window** | 🔄 In Progress | GtkApplication, dark SCADA CSS, header bar, sidebar, data grid, status bar |
| **Phase 4: GTK4 UI — Dialogs + Commands** | 🔄 In Progress | Command control panel, comm log, traffic monitor, edit slave/new connection dialogs |
| **Phase 5: Integration + Polish** | ⏳ Pending | Multi-connection, config save/reload, CSV export, end-to-end testing |

## Supported ASDU Types

### Monitoring (received from server)

- **Single-point** — `M_SP_NA_1`, `M_SP_TA_1`, `M_SP_TB_1`
- **Double-point** — `M_DP_NA_1`, `M_DP_TA_1`, `M_DP_TB_1`
- **Step position** — `M_ST_NA_1`, `M_ST_TA_1`, `M_ST_TB_1`
- **Bitstring** — `M_BO_NA_1`, `M_BO_TA_1`, `M_BO_TB_1`
- **Measured (normalized)** — `M_ME_NA_1`, `M_ME_ND_1`, `M_ME_TA_1`, `M_ME_TD_1`
- **Measured (scaled)** — `M_ME_NB_1`, `M_ME_TB_1`, `M_ME_TE_1`
- **Measured (float)** — `M_ME_NC_1`, `M_ME_TC_1`, `M_ME_TF_1`
- **Integrated totals** — `M_IT_NA_1`, `M_IT_TA_1`, `M_IT_TB_1`
- **Protection events** — `M_EP_TA_1` through `M_EP_TF_1`
- **End of initialization** — `M_EI_NA_1`

### Commands (sent to server)

- **Single command** — `C_SC_NA_1`, `C_SC_TA_1`
- **Double command** — `C_DC_NA_1`, `C_DC_TA_1`
- **Step command** — `C_RC_NA_1`, `C_RC_TA_1`
- **Setpoint (normalized)** — `C_SE_NA_1`, `C_SE_TA_1`
- **Setpoint (scaled)** — `C_SE_NB_1`, `C_SE_TB_1`
- **Setpoint (float)** — `C_SE_NC_1`, `C_SE_TC_1`
- **Bitstring command** — `C_BO_NA_1`, `C_BO_TA_1`
- **System commands** — Interrogation (`C_IC_NA_1`), Counter interrogation (`C_CI_NA_1`), Read (`C_RD_NA_1`), Clock sync (`C_CS_NA_1`), Test (`C_TS_NA_1`/`C_TS_TA_1`), Reset process (`C_RP_NA_1`), Delay acquisition (`C_CD_NA_1`)

## Important Notes

- **Stack size**: `IEC104_StationConfig` is ~775 KB (32 groups × 256 points). Configs and connections **must** be `static` or heap-allocated, never stack-local.
- **Threading**: Connection runs in a background thread. GUI updates use `g_idle_add` for thread safety.
- **Windows**: Force unbuffered stdout in `main.c` because MSYS2/mintty pipes use full buffering.
- **Auto-reconnect**: Configurable interval and max attempts; state machine handles CONNECTING → CONNECTED → RECONNECTING transitions.

## Key Files

| Path | Description |
|------|-------------|
| `CMakeLists.txt` | Build configuration — all targets and dependencies |
| `station_config.json` | Default connection configuration |
| `src/iec104_types.h` | Central type definitions for the entire project |
| `src/iec104_conn.h` | Connection manager API |
| `src/iec104_cmd.h` | Command dispatch API |
| `docs/task.md` | Master task checklist for all phases |
| `docs/implementation_plan.md` | Command control panel implementation plan |
| `res/style.css` | GTK4 dark SCADA theme |
| `iec104_traffic.csv` | Runtime protocol traffic log (generated) |
