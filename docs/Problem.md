Role
​Act as a Senior Embedded Systems Engineer and SCADA Communications Expert.
​Context Update
​I have confirmed the root cause of the monitoring issue. The current IEC104 Client successfully receives and displays M_SP_NA_1 (Single Point/Binary), However, the system failed to process Measured Values/Set Points (Analog), When my IEC 104 client software first ran, I did not see the value received by the IEC 104 client. To resolve this, I had to change the value from the server side, and my IEC 104 client was then able to read the value on the IOA that I updated from the server side. The reading method should run in real time, with the client continuously updating the current value, even though the value on the server side is on standby. 
​To fix this, the Client needs to be able to parse various "Measured Value" ASDU types that the Server sends.
​Technical Objective
​Modify the Client's ASDUReceivedHandler to detect, parse, and pass analog values to the GTK4 UI.
​AI Tasks
​Please analyze the codebase and implement the following:
​Implement Measured Value Parsing (Backend):
Update the ASDU callback to handle these specific types: see document IOA_List.md
​Data Structure Synchronization:
​Review the struct or object that stores the monitoring data.
​Ensure it has a field for double or float values (not just bool).
​Ensure it can store the Quality Descriptor (to show if the value is Valid, Overflow, or Blocked).
​Dynamic UI Mapping (GTK4):
​Update the UI refresh logic (likely inside a g_idle_add callback).
​Use a conditional format: If the type is Binary, show "ON/OFF"; if the type is Measured, show the numerical value (e.g., "%.2f").
​Execution Rule
​Provide the code for Task 1 and 2 first. Focus on how you extract the value from MeasuredValueScaled or MeasuredValueShortFloatingPoint using the lib60870 API. Once I confirm the backend logic is correct, we will proceed to the GTK4 UI integration.