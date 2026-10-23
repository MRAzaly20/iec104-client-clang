**Role:** You are a Senior Embedded Systems Architect and GTK4 UI Expert.

**Current Task:** I am adding a Single-Line Diagram (SLD) monitoring and control feature to my IEC 104 Client. The SLD must visualize an Overhead Line (OHL) Bay, a Transformer Bay, and a Bus Coupler.

**Technical Requirements:**

1. **Rendering with Cairo:** Use `GtkDrawingArea` and the Cairo graphics library to draw the SLD. Create reusable drawing functions for standard IEC symbols:
   - Busbars (thick lines)
   - Transformers (intersecting circles)
   - Circuit Breakers (CB) (squares that change fill state)
   - Disconnecting Switches (DS) (lines that pivot open/closed).
2. **Hit Detection (Interactivity):** Implement a `GtkGestureClick` controller on the drawing area. When the user clicks on the coordinates of a CB or DS, a dialog should prompt for an "Open" or "Close" execution.
3. **Protocol Mapping:** > \* Provide the C logic to map UI switch operations to IEC 104 Control Commands (specifically handling ASDU Type 45/46/58).
   - Provide the logic to update the on-screen text for V, I, and P when receiving telemetry measurements (ASDU Type 13/36).
4. **Topological State:** Create a C `struct` to manage the state of the bays. If a bay's DS or CB is open, the logic should immediately reflect that the connected segment is de-energized.

**Deliverables needed:**

- The GTK4/Cairo drawing routines for the OHL, Transformer, and Bus Coupler bays.
- The coordinate-mapping logic for user clicks on the drawing area.
- The struct definitions for the SLD topology and IEC 104 mapping.
