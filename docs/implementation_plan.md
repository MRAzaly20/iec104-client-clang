# Command Control Panel — Full Implementation Plan

Redesign the existing flat command strip into a professional two-panel Command Control + QOC reference layout, matching the provided reference screenshots. Add QU qualifier support to the backend and time-tag support for bitstring commands.

## Proposed Changes

### Backend: Command Layer

#### [MODIFY] [iec104_cmd.h](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.h)

Add `int qu` parameter to all 7 command functions. Also add `bool select` and `bool with_time` to [iec104_cmd_bitstring](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#196-213) for C_BO_TA_1 support.

```diff
-IEC104_CmdResult iec104_cmd_single(IEC104_Connection *c, int ioa,
-                                    bool state, bool select, bool with_time);
+IEC104_CmdResult iec104_cmd_single(IEC104_Connection *c, int ioa,
+                                    bool state, bool select, int qu, bool with_time);
```

Same pattern for [_double](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#52-78), [_step](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#79-105), [_setpoint_normalized](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#106-135), [_setpoint_scaled](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#136-165), [_setpoint_float](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#166-195). For bitstring:

```diff
-IEC104_CmdResult iec104_cmd_bitstring(IEC104_Connection *c, int ioa,
-                                       uint32_t value);
+IEC104_CmdResult iec104_cmd_bitstring(IEC104_Connection *c, int ioa,
+                                       uint32_t value, bool with_time);
```

#### [MODIFY] [iec104_cmd.c](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c)

- Pass `qu` to all [_create](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_gui.c#1218-1280) calls (replaces hardcoded `0`)
- Add time-tagged variant for bitstring using `Bitstring32CommandWithCP56Time2a_create`

---

### GUI: New Command Control Panel

#### [MODIFY] [iec104_gui.h](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_gui.h)

Replace existing [cmd_panel](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_gui.c#367-420) widgets with new command control panel fields:

| Widget | Purpose |
|--------|---------|
| `cmd_frame` | Main frame "⚡ Command Control" |
| `cmd_type_badge` | ASDU type badge label (e.g. "C_SC_NA_1") |
| `cmd_target_frame` | TARGET info box (IOA + name) |
| `cmd_ioa_label` | Shows "IOA: X  Name" |
| `cmd_status_dot` | Green dot for Ready/connected |
| `cmd_qu_combo` | QU qualifier dropdown (4 options) |
| `cmd_se_combo` | S/E dropdown (Execute / Select) |
| `cmd_time_check` | Time tag checkbox |
| `cmd_value_entry` | Value input (for setpoints/bitstring) |
| `cmd_action_box` | Container for action buttons |
| `cmd_qoc_frame` | QOC reference info panel |

#### [MODIFY] [iec104_gui.c](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_gui.c)

**Replace [build_cmd_panel](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_gui.c#367-420)** (~50 lines → ~180 lines) with new implementation:

1. **Left Panel — Command Control** (`cmd_frame`):
   - Header row: "⚡ Command Control" title + type badge (e.g. `C_DC_NA_1`)
   - TARGET info box: shows selected IOA + point name + Ready status dot
   - Qualifier row: "Qualifier:" label + QU dropdown (No Additional/Short Pulse/Long Pulse/Persistent)
   - S/E row: Execute (0) / Select (1) toggle
   - Time tag checkbox
   - Value entry (only visible for setpoint/bitstring types)
   - Action buttons: type-specific:
     - **SC**: `↑ ON` (green), `↓ OFF` (red)
     - **DC**: `↑ ON` (green), `↓ OFF` (red), `◆ INDETERMINATE` (olive), `◇ INTERMEDIATE` (yellow)
     - **RC**: `▲ UP` (blue), `▼ DOWN` (orange)
     - **SE_NA/NB/NC**: Value entry + `⚡ Send` (blue) + range hint label
     - **BO**: Hex value entry + `⚡ Send` (blue)

2. **Right Panel — QOC Reference** (`cmd_qoc_frame`):
   - Static reference info with monospace formatting showing QU/S/E bit fields
   - Matches the reference screenshot appearance

**Replace [on_cmd_send](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_gui.c#285-366)** with type-specific action button callbacks:
- `on_cmd_sc_action(state)` — calls [iec104_cmd_single](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#25-51) with ON/OFF
- `on_cmd_dc_action(dcs)` — calls [iec104_cmd_double](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#52-78) with 1/2/0/3
- `on_cmd_rc_action(step)` — calls [iec104_cmd_step](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/src/iec104_cmd.c#79-105) with 1/2
- `on_cmd_se_action()` — reads value entry, dispatches to normalized/scaled/float
- `on_cmd_bo_action()` — reads hex value, dispatches to bitstring

**Add `on_cmd_type_changed`** callback — switches visible controls per type selection.

**Add data-grid row click handler** — when a row is clicked, auto-populate the command panel with the IOA and detect the command type.

---

### CSS Styling

#### [MODIFY] [style.css](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/res/style.css)

Add new CSS classes:

| Class | Purpose |
|-------|---------|
| `.cmd-panel` | Main command container with dark blue gradient |
| `.cmd-badge` | Type badge (blue pill) |
| `.cmd-target` | Target info box (dark inset) |
| `.cmd-status-dot` | Green status indicator |
| `.cmd-btn-on` | Green ON button |
| `.cmd-btn-off` | Red OFF button |
| `.cmd-btn-indet` | Olive indeterminate button |
| `.cmd-btn-inter` | Yellow intermediate button |
| `.cmd-btn-send` | Blue send button |
| `.cmd-qoc` | QOC info panel (dark monospace) |

## Verification Plan

### Build Verification
1. Run `cmake --build . 2>&1` from the project root to confirm clean compilation
2. Check for zero errors and zero warnings related to changed files

### Manual UI Verification
The user should launch the built [iec104_client_gui.exe](file:///c:/nexus-project/nexus-api/iiot_gateway_project/services/device_management/protocol/iec104-client-c/iec104_client_gui.exe) and verify:
1. The new Command Control panel appears below the data grid
2. The QOC info panel appears to the right of the command panel
3. Changing command type dropdown switches the visible action buttons
4. QU qualifier dropdown shows 4 options
5. ON/OFF buttons appear for SC type; ON/OFF/INDETERMINATE/INTERMEDIATE for DC type
6. Setpoint types show value entry with range hints
7. Clicking a data grid row populates the command panel target
