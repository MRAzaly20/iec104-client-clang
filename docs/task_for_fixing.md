Task: Refactor the IEC 104 Client C implementation to remediate identified high-priority vulnerabilities.

1. Fix VULN-1 & VULN-4: Heap & UI DoS (Memory Management)
Action: Implement a Lockless Ring Buffer to decouple the protocol thread from the GTK main thread.

Requirement: Instead of calling g_idle_add for every single ASDU element, the protocol thread must push updates into a fixed-size ring buffer (e.g., 4096 entries).

Requirement: Use a single, recurring g_timeout_add or a throttled g_idle_add in the GTK thread to drain the entire buffer in one batch.

Requirement: Implement a strict cap on auto-created points (e.g., MAX_AUTO_POINTS = 64) to prevent memory exhaustion and UI slowdowns.

2. Fix VULN-3: TOCTOU Race (Data Integrity)
Action: Establish a Single Source of Truth for the data model.

Requirement: Remove all direct writes to the c->config data model within the protocol thread's fire_point function.

Requirement: All data model updates (point values, quality flags, timestamps) must occur exclusively within the GTK main thread during the ring buffer drain process. This ensures the GUI and the protocol logic always see the same state.

3. Fix VULN-5: COT Validation (Protocol Compliance)
Action: Implement a Cause of Transmission (COT) Filter.

Requirement: In asduReceivedHandler, check the COT of every incoming monitoring ASDU.

Requirement: Reject and log any ASDU that does not have a valid monitoring COT (e.g., Spontaneous, Requested, or Interrogation response).

Requirement: Specifically block undefined or command-direction-only COTs (like Activation) from updating the monitoring data model.