The IEC 60870-5-104 client application shall use `station_config.json` as the single source of truth for all station configurations.

### 1. Initial Load Behavior

* On application startup, the system shall automatically load and apply all configurations from `station_config.json` before establishing any station connections.

### 2. New / Edit Connection Workflow

* When a user creates or edits a station via the “New” or “Edit” feature:

1. The updated configuration must be saved to `station_config.json`.
2. The system shall reload the updated configuration from the file.
3. Only the affected station(s) should be reinitialized (not all stations, unless required).

### 3. Auto-Reconnect Mechanism

* After reloading the configuration:
* The client shall gracefully disconnect the affected station (if connected).
* Reinitialize the IEC104 session:
  *  Establish TCP connection
  *  Send STARTDT
  *  Trigger General Interrogation (GI)
* Reconnect automatically using the updated configuration.

### 4. Change Detection

* Reconnection shall only occur if there are actual changes in:
* IP / Port
* Common Address (CA)
* IOA mapping
* ASDU configuration

### 5. Error Handling

* If `station_config.json` is invalid:
* Reject reload
* Log error
* Keep previous valid configuration active
* If reconnection fails:
* Retry using configurable backoff strategy
* Log connection state in monitoring tab

### 6. UI Synchronization

* The monitoring tab must subscribe to the updated station instance after reload.
