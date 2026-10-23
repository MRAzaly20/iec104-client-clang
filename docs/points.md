A. Monitoring / Information Objects (M_)
Data sent from RTU/Field to SCADA/Master.

Single Point Information (Status ON/OFF)
 M_SP_NA_1 (Type 1) – Single-point information (without time)
 M_SP_TA_1 (Type 2) – Single-point information with time tag CP24Time2a
 M_SP_TB_1 (Type 3) – Single-point information with time tag CP56Time2a

Double Point Information (Status OPEN/CLOSE)
 M_DP_NA_1 (Type 9) – Double-point information (without time)
 M_DP_TA_1 (Type 10) – Double-point information with time tag CP24Time2a
 M_DP_TB_1 (Type 11) – Double-point information with time tag CP56Time2a

Step Position Information (Tap Changer)
 M_ST_NA_1 (Type 30) – Step position information (without time)
 M_ST_TA_1 (Type 31) – Step position information with time tag CP24Time2a
 M_ST_TB_1 (Type 32) – Step position information with time tag CP56Time2a

Bitstring of 32 Bits
 M_BO_NA_1 (Type 35) – Bitstring of 32 bits (without time)
 M_BO_TA_1 (Type 36) – Bitstring of 32 bits with time tag CP24Time2a
 M_BO_TB_1 (Type 37) – Bitstring of 32 bits with time tag CP56Time2a

Measured Values (Normalized)
 M_ME_NA_1 (Type 39) – Measured value, normalized value (without time)
 M_ME_NB_1 (Type 40) – Measured value, normalized value with time tag CP24Time2a
 M_ME_NC_1 (Type 41) – Measured value, normalized value with time tag CP56Time2a

Measured Values (Scaled)
 M_ME_ND_1 (Type 42) – Measured value, scaled value (without time)
 M_ME_NE_1 (Type 43) – Measured value, scaled value with time tag CP24Time2a
 M_ME_NF_1 (Type 44) – Measured value, scaled value with time tag CP56Time2a

Measured Values (Short Floating Point)
 M_ME_NG_1 (Type 45) – Measured value, short floating point number (without time)
 M_ME_NH_1 (Type 46) – Measured value, short floating point number with time tag CP24Time2a
 M_ME_NI_1 (Type 47) – Measured value, short floating point number with time tag CP56Time2a

Integrated Totals (Energy/Counters)
 M_IT_NA_1 (Type 60) – Integrated totals (without time)
 M_IT_TA_1 (Type 61) – Integrated totals with time tag CP24Time2a
 M_IT_TB_1 (Type 62) – Integrated totals with time tag CP56Time2a

Event Counters (Protection Equipment)
 M_EP_TA_1 (Type 63) – Event of protection equipment with time tag CP24Time2a
 M_EP_TB_1 (Type 64) – Event of protection equipment with time tag CP56Time2a
 M_EP_TC_1 (Type 65) – Packed start events of protection equipment with time tag CP56Time2a

Initialization Status
 M_EI_NA_1 (Type 70) – End of initialization

---

### B. System Control & Commands (C_)
Commands sent from SCADA/Master to RTU/Field.

System Management
 C_IC_NA_1 (Type 100) – General interrogation command
 C_CI_NA_1 (Type 101) – Counter interrogation command
 C_RD_NA_1 (Type 102) – Read command
 C_CS_NA_1 (Type 103) – Clock synchronization command
 C_TS_NA_1 (Type 104) – Test command
 C_RP_NA_1 (Type 105) – Reset process command
 C_CD_NA_1 (Type 106) – Delay acquisition command

Control Commands (Single, Double, Step)
 C_SC_NA_1 (Type 45) – Single command (without time)
 C_SC_TA_1 (Type 48) – Single command with time tag CP56Time2a
 C_DC_NA_1 (Type 46) – Double command (without time)
 C_DC_TA_1 (Type 49) – Double command with time tag CP56Time2a
 C_RC_NA_1 (Type 47) – Regulating step command (without time)
 C_RC_TA_1 (Type 50) – Regulating step command with time tag CP56Time2a

Set-point Commands (Normalized, Scaled, Float)
 C_SE_NA_1 (Type 51) – Set-point command, normalized value (without time)
 C_SE_TA_1 (Type 54) – Set-point command, normalized value with time tag CP56Time2a
 C_SE_NB_1 (Type 52) – Set-point command, scaled value (without time)
 C_SE_TB_1 (Type 55) – Set-point command, scaled value with time tag CP56Time2a
 C_SE_NC_1 (Type 53) – Set-point command, short floating point number (without time)
 C_SE_TC_1 (Type 56) – Set-point command, short floating point number with time tag CP56Time2a

Bitstring Command
 C_BO_NA_1 (Type 57) – Bitstring of 32 bits command (without time)
 C_BO_TA_1 (Type 58) – Bitstring of 32 bits command with time tag CP56Time2a