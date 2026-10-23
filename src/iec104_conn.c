/**
 * @file iec104_conn.c
 * @brief IEC 104 Connection Manager — lib60870 + threading
 *
 * Manages connection lifecycle: connect → STARTDT → GI → monitor loop,
 * with auto-reconnect and periodic time sync.
 */
#include "iec104_conn.h"
#include "iec104_model.h"
#include "iec104_log.h"

#include "cs104_connection.h"
#include "hal_time.h"
#include "hal_thread.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ---- Cross-platform mutex helpers ---- */
#ifdef _WIN32
#  define MTX_INIT(m)    InitializeCriticalSection(&(m))
#  define MTX_LOCK(m)    EnterCriticalSection(&(m))
#  define MTX_UNLOCK(m)  LeaveCriticalSection(&(m))
#  define MTX_DESTROY(m) DeleteCriticalSection(&(m))
#else
#  define MTX_INIT(m)    pthread_mutex_init(&(m), NULL)
#  define MTX_LOCK(m)    pthread_mutex_lock(&(m))
#  define MTX_UNLOCK(m)  pthread_mutex_unlock(&(m))
#  define MTX_DESTROY(m) pthread_mutex_destroy(&(m))
#endif

/* ============================================================================
 * Internal helpers: fire callbacks
 * ========================================================================= */
static void fire_state(IEC104_Connection *c, IEC104_ConnState s)
{
    c->state = s;
    if (c->on_state_change)
        c->on_state_change(c->id, s, c->cb_user_data);
}

static void fire_log(IEC104_Connection *c, IEC104_LogLevel lv,
                     const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    iec104_log(lv, "[%s] %s", c->id, buf);
    if (c->on_log)
        c->on_log(c->id, lv, buf, c->cb_user_data);
}

static void fire_point(IEC104_Connection *c, int ioa, int tid,
                       IEC104_Value val, IEC104_Quality q, uint64_t ts)
{
    /* VULN-3 fix: Do NOT update c->config from the protocol thread.
     * All data-model writes now happen exclusively in the GTK main
     * thread (ring-buffer drain) to eliminate the TOCTOU race. */

    if (c->on_point_update)
        c->on_point_update(c->id, ioa, val, q, ts, tid, c->cb_user_data);
}

/* ============================================================================
 * lib60870 Connection Handler callback
 * ========================================================================= */
static void connectionHandler(void *param, CS104_Connection con,
                               CS104_ConnectionEvent event)
{
    IEC104_Connection *c = (IEC104_Connection *)param;
    (void)con;

    switch (event) {
    case CS104_CONNECTION_OPENED:
        fire_log(c, LOG_SUCCESS, "TCP connection opened");
        fire_state(c, CONN_CONNECTED);
        break;
    case CS104_CONNECTION_CLOSED:
        fire_log(c, LOG_WARNING, "Connection closed");
        fire_state(c, CONN_DISCONNECTED);
        break;
    case CS104_CONNECTION_FAILED:
        fire_log(c, LOG_ERROR, "Connection failed");
        fire_state(c, CONN_ERROR);
        break;
    case CS104_CONNECTION_STARTDT_CON_RECEIVED:
        fire_log(c, LOG_INFO, "STARTDT_CON received");
        break;
    case CS104_CONNECTION_STOPDT_CON_RECEIVED:
        fire_log(c, LOG_INFO, "STOPDT_CON received");
        break;
    }
}

/* ============================================================================
 * ASDU Received Handler — full monitoring-type dispatcher
 *
 * CP56Time2a variants (TB_1) are split into separate branches so they use
 * the correct lib60870 destructor.  getValue/getQuality are accessed via
 * the base-type cast (lib60870 inheritance convention).
 * ========================================================================= */
static bool asduReceivedHandler(void *param, int address, CS101_ASDU asdu)
{
    IEC104_Connection *c = (IEC104_Connection *)param;
    (void)address;
    TypeID tid = CS101_ASDU_getTypeID(asdu);
    int ne    = CS101_ASDU_getNumberOfElements(asdu);

    /* Diagnostic: log every incoming ASDU so we can verify data flow */
    fprintf(stderr, "[ASDU] %s: type=%d (%s) n=%d COT=%d\n",
            c->id, tid, TypeID_toString(tid), ne, CS101_ASDU_getCOT(asdu));
    fire_log(c, LOG_INFO, "ASDU rx: type=%d (%s) n=%d COT=%d",
             tid, TypeID_toString(tid), ne, CS101_ASDU_getCOT(asdu));

    /* ================================================================
     * VULN-5 fix: COT validation for monitoring-direction ASDUs.
     * Per IEC 60870-5-101 §7.2.3, monitoring types (1–40, 70) must
     * carry a valid monitoring COT.  Command/system ASDUs (>=45) are
     * not filtered here — they have their own COT semantics.
     * ============================================================= */
    if ((tid >= 1 && tid <= 40) || tid == M_EI_NA_1) {
        CS101_CauseOfTransmission cot = CS101_ASDU_getCOT(asdu);
        bool cot_valid = false;

        switch (cot) {
        case CS101_COT_PERIODIC:               /*  1 */
        case CS101_COT_BACKGROUND_SCAN:        /*  2 */
        case CS101_COT_SPONTANEOUS:            /*  3 */
        case CS101_COT_INITIALIZED:            /*  4 */
        case CS101_COT_REQUEST:                /*  5 */
        case CS101_COT_RETURN_INFO_REMOTE:     /* 11 */
        case CS101_COT_RETURN_INFO_LOCAL:      /* 12 */
            cot_valid = true;
            break;
        default:
            /* GI groups 20-36, Counter groups 37-41 */
            if (cot >= CS101_COT_INTERROGATED_BY_STATION &&
                cot <= CS101_COT_REQUESTED_BY_GROUP_4_COUNTER)
                cot_valid = true;
            break;
        }

        if (!cot_valid) {
            fire_log(c, LOG_WARNING,
                     "Non-standard COT=%d for monitoring ASDU type=%d (%s)"
                     " — processing anyway for interoperability",
                     cot, tid, TypeID_toString(tid));
            /* VULN-5: Log but do NOT drop — many real-world devices and
             * simulators send monitoring types with non-standard COTs.
             * Dropping would break interoperability. */
        }
    }

    /* ---- M_SP_NA_1 / M_SP_TA_1  Single Point (base + CP24) ---- */
    if (tid == M_SP_NA_1 || tid == M_SP_TA_1) {
        for (int i = 0; i < ne; i++) {
            SinglePointInformation io =
                (SinglePointInformation)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_BOOL,
                               .v.b = SinglePointInformation_getValue(io) };
            IEC104_Quality q = SinglePointInformation_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            SinglePointInformation_destroy(io);
        }
    }
    /* ---- M_SP_TB_1  Single Point + CP56Time2a ---- */
    else if (tid == M_SP_TB_1) {
        for (int i = 0; i < ne; i++) {
            SinglePointWithCP56Time2a io =
                (SinglePointWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_BOOL,
                               .v.b = SinglePointInformation_getValue((SinglePointInformation)io) };
            IEC104_Quality q = SinglePointInformation_getQuality((SinglePointInformation)io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            SinglePointWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_DP_NA_1 / M_DP_TA_1  Double Point (base + CP24) ---- */
    else if (tid == M_DP_NA_1 || tid == M_DP_TA_1) {
        for (int i = 0; i < ne; i++) {
            DoublePointInformation io =
                (DoublePointInformation)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_DOUBLE_POINT,
                               .v.i = DoublePointInformation_getValue(io) };
            IEC104_Quality q = DoublePointInformation_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            DoublePointInformation_destroy(io);
        }
    }
    /* ---- M_DP_TB_1  Double Point + CP56Time2a ---- */
    else if (tid == M_DP_TB_1) {
        for (int i = 0; i < ne; i++) {
            DoublePointWithCP56Time2a io =
                (DoublePointWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_DOUBLE_POINT,
                               .v.i = DoublePointInformation_getValue((DoublePointInformation)io) };
            IEC104_Quality q = DoublePointInformation_getQuality((DoublePointInformation)io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            DoublePointWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_ST_NA_1 / M_ST_TA_1  Step Position (base + CP24) ---- */
    else if (tid == M_ST_NA_1 || tid == M_ST_TA_1) {
        for (int i = 0; i < ne; i++) {
            StepPositionInformation io =
                (StepPositionInformation)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_INT32,
                               .v.i = StepPositionInformation_getValue(io) };
            IEC104_Quality q = StepPositionInformation_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            StepPositionInformation_destroy(io);
        }
    }
    /* ---- M_ST_TB_1  Step Position + CP56Time2a ---- */
    else if (tid == M_ST_TB_1) {
        for (int i = 0; i < ne; i++) {
            StepPositionWithCP56Time2a io =
                (StepPositionWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_INT32,
                               .v.i = StepPositionInformation_getValue((StepPositionInformation)io) };
            IEC104_Quality q = StepPositionInformation_getQuality((StepPositionInformation)io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            StepPositionWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_BO_NA_1 / M_BO_TA_1  Bitstring (base + CP24) ---- */
    else if (tid == M_BO_NA_1 || tid == M_BO_TA_1) {
        for (int i = 0; i < ne; i++) {
            BitString32 io = (BitString32)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_UINT32,
                               .v.bits = BitString32_getValue(io) };
            fire_point(c, ioa, tid, v, QUALITY_GOOD, Hal_getTimeInMs());
            BitString32_destroy(io);
        }
    }
    /* ---- M_BO_TB_1  Bitstring + CP56Time2a ---- */
    else if (tid == M_BO_TB_1) {
        for (int i = 0; i < ne; i++) {
            Bitstring32WithCP56Time2a io =
                (Bitstring32WithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_UINT32,
                               .v.bits = BitString32_getValue((BitString32)io) };
            fire_point(c, ioa, tid, v, QUALITY_GOOD, Hal_getTimeInMs());
            Bitstring32WithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_ME_NA_1 / M_ME_TA_1  Measured Normalized (base + CP24) ---- */
    else if (tid == M_ME_NA_1 || tid == M_ME_TA_1) {
        for (int i = 0; i < ne; i++) {
            MeasuredValueNormalized io =
                (MeasuredValueNormalized)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_FLOAT,
                               .v.f = MeasuredValueNormalized_getValue(io) };
            IEC104_Quality q = MeasuredValueNormalized_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            MeasuredValueNormalized_destroy(io);
        }
    }
    /* ---- M_ME_TD_1  Measured Normalized + CP56Time2a ---- */
    else if (tid == M_ME_TD_1) {
        for (int i = 0; i < ne; i++) {
            MeasuredValueNormalizedWithCP56Time2a io =
                (MeasuredValueNormalizedWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_FLOAT,
                               .v.f = MeasuredValueNormalized_getValue((MeasuredValueNormalized)io) };
            IEC104_Quality q = MeasuredValueNormalized_getQuality((MeasuredValueNormalized)io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            MeasuredValueNormalizedWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_ME_ND_1  Normalized WITHOUT quality descriptor ---- */
    else if (tid == M_ME_ND_1) {
        for (int i = 0; i < ne; i++) {
            MeasuredValueNormalized io =
                (MeasuredValueNormalized)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_FLOAT,
                               .v.f = MeasuredValueNormalized_getValue(io) };
            fire_point(c, ioa, tid, v, QUALITY_GOOD, Hal_getTimeInMs());
            MeasuredValueNormalized_destroy(io);
        }
    }
    /* ---- M_ME_NB_1 / M_ME_TB_1  Measured Scaled (base + CP24) ---- */
    else if (tid == M_ME_NB_1 || tid == M_ME_TB_1) {
        for (int i = 0; i < ne; i++) {
            MeasuredValueScaled io =
                (MeasuredValueScaled)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_INT32,
                               .v.i = MeasuredValueScaled_getValue(io) };
            IEC104_Quality q = MeasuredValueScaled_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            MeasuredValueScaled_destroy(io);
        }
    }
    /* ---- M_ME_TE_1  Measured Scaled + CP56Time2a ---- */
    else if (tid == M_ME_TE_1) {
        for (int i = 0; i < ne; i++) {
            MeasuredValueScaledWithCP56Time2a io =
                (MeasuredValueScaledWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_INT32,
                               .v.i = MeasuredValueScaled_getValue((MeasuredValueScaled)io) };
            IEC104_Quality q = MeasuredValueScaled_getQuality((MeasuredValueScaled)io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            MeasuredValueScaledWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_ME_NC_1 / M_ME_TC_1  Measured Short Float (base + CP24) ---- */
    else if (tid == M_ME_NC_1 || tid == M_ME_TC_1) {
        for (int i = 0; i < ne; i++) {
            MeasuredValueShort io =
                (MeasuredValueShort)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_FLOAT,
                               .v.f = MeasuredValueShort_getValue(io) };
            IEC104_Quality q = MeasuredValueShort_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            MeasuredValueShort_destroy(io);
        }
    }
    /* ---- M_ME_TF_1  Measured Short Float + CP56Time2a ---- */
    else if (tid == M_ME_TF_1) {
        for (int i = 0; i < ne; i++) {
            MeasuredValueShortWithCP56Time2a io =
                (MeasuredValueShortWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            IEC104_Value v = { .type = VAL_FLOAT,
                               .v.f = MeasuredValueShort_getValue((MeasuredValueShort)io) };
            IEC104_Quality q = MeasuredValueShort_getQuality((MeasuredValueShort)io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            MeasuredValueShortWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_IT_NA_1 / M_IT_TA_1  Integrated Totals (base + CP24) ---- */
    else if (tid == M_IT_NA_1 || tid == M_IT_TA_1) {
        for (int i = 0; i < ne; i++) {
            IntegratedTotals io =
                (IntegratedTotals)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            BinaryCounterReading bcr = IntegratedTotals_getBCR(io);
            IEC104_Value v = { .type = VAL_INT32,
                               .v.i = BinaryCounterReading_getValue(bcr) };
            fire_point(c, ioa, tid, v, QUALITY_GOOD, Hal_getTimeInMs());
            IntegratedTotals_destroy(io);
        }
    }
    /* ---- M_IT_TB_1  Integrated Totals + CP56Time2a ---- */
    else if (tid == M_IT_TB_1) {
        for (int i = 0; i < ne; i++) {
            IntegratedTotalsWithCP56Time2a io =
                (IntegratedTotalsWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            BinaryCounterReading bcr = IntegratedTotals_getBCR((IntegratedTotals)io);
            IEC104_Value v = { .type = VAL_INT32,
                               .v.i = BinaryCounterReading_getValue(bcr) };
            fire_point(c, ioa, tid, v, QUALITY_GOOD, Hal_getTimeInMs());
            IntegratedTotalsWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_EP_TA_1  Event of Protection Equipment (CP24) ---- */
    else if (tid == M_EP_TA_1) {
        for (int i = 0; i < ne; i++) {
            EventOfProtectionEquipment io =
                (EventOfProtectionEquipment)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            SingleEvent ev = EventOfProtectionEquipment_getEvent(io);
            IEC104_Value v = { .type = VAL_INT32, .v.i = (int32_t)ev };
            fire_point(c, ioa, tid, v, QUALITY_GOOD, Hal_getTimeInMs());
            EventOfProtectionEquipment_destroy(io);
        }
    }
    /* ---- M_EP_TD_1  Event of Protection Equipment + CP56Time2a ---- */
    else if (tid == M_EP_TD_1) {
        for (int i = 0; i < ne; i++) {
            EventOfProtectionEquipmentWithCP56Time2a io =
                (EventOfProtectionEquipmentWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            SingleEvent ev = EventOfProtectionEquipmentWithCP56Time2a_getEvent(io);
            IEC104_Value v = { .type = VAL_INT32, .v.i = (int32_t)ev };
            fire_point(c, ioa, tid, v, QUALITY_GOOD, Hal_getTimeInMs());
            EventOfProtectionEquipmentWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_EP_TB_1  Packed Start Events of Protection Equipment (CP24) ---- */
    else if (tid == M_EP_TB_1) {
        for (int i = 0; i < ne; i++) {
            PackedStartEventsOfProtectionEquipment io =
                (PackedStartEventsOfProtectionEquipment)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            StartEvent ev = PackedStartEventsOfProtectionEquipment_getEvent(io);
            IEC104_Value v = { .type = VAL_INT32, .v.i = (int32_t)ev };
            IEC104_Quality q = PackedStartEventsOfProtectionEquipment_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            PackedStartEventsOfProtectionEquipment_destroy(io);
        }
    }
    /* ---- M_EP_TE_1  Packed Start Events + CP56Time2a ---- */
    else if (tid == M_EP_TE_1) {
        for (int i = 0; i < ne; i++) {
            PackedStartEventsOfProtectionEquipmentWithCP56Time2a io =
                (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            StartEvent ev = PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent(io);
            IEC104_Value v = { .type = VAL_INT32, .v.i = (int32_t)ev };
            IEC104_Quality q = PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            PackedStartEventsOfProtectionEquipmentWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_EP_TC_1  Packed Output Circuit Info (CP24) ---- */
    else if (tid == M_EP_TC_1) {
        for (int i = 0; i < ne; i++) {
            PackedOutputCircuitInfo io =
                (PackedOutputCircuitInfo)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            OutputCircuitInfo oci = PackedOutputCircuitInfo_getOCI(io);
            IEC104_Value v = { .type = VAL_INT32, .v.i = (int32_t)oci };
            IEC104_Quality q = PackedOutputCircuitInfo_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            PackedOutputCircuitInfo_destroy(io);
        }
    }
    /* ---- M_EP_TF_1  Packed Output Circuit Info + CP56Time2a ---- */
    else if (tid == M_EP_TF_1) {
        for (int i = 0; i < ne; i++) {
            PackedOutputCircuitInfoWithCP56Time2a io =
                (PackedOutputCircuitInfoWithCP56Time2a)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            OutputCircuitInfo oci = PackedOutputCircuitInfoWithCP56Time2a_getOCI(io);
            IEC104_Value v = { .type = VAL_INT32, .v.i = (int32_t)oci };
            IEC104_Quality q = PackedOutputCircuitInfoWithCP56Time2a_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            PackedOutputCircuitInfoWithCP56Time2a_destroy(io);
        }
    }
    /* ---- M_PS_NA_1  Packed Single Point with SCD ---- */
    else if (tid == M_PS_NA_1) {
        for (int i = 0; i < ne; i++) {
            PackedSinglePointWithSCD io =
                (PackedSinglePointWithSCD)CS101_ASDU_getElement(asdu, i);
            if (!io) continue;
            int ioa = InformationObject_getObjectAddress((InformationObject)io);
            StatusAndStatusChangeDetection scd =
                PackedSinglePointWithSCD_getSCD(io);
            /* Store 16-bit status (STn) in the lower half, 16-bit change
             * detection (CDn) in the upper half of a 32-bit bitstring. */
            uint16_t stn = StatusAndStatusChangeDetection_getSTn(scd);
            uint16_t cdn = StatusAndStatusChangeDetection_getCDn(scd);
            IEC104_Value v = { .type = VAL_UINT32,
                               .v.bits = ((uint32_t)cdn << 16) | stn };
            IEC104_Quality q = PackedSinglePointWithSCD_getQuality(io) == 0
                                   ? QUALITY_GOOD : QUALITY_BAD;
            fire_point(c, ioa, tid, v, q, Hal_getTimeInMs());
            PackedSinglePointWithSCD_destroy(io);
        }
    }
    /* ---- System ASDUs ---- */
    else if (tid == C_IC_NA_1 || tid == C_CI_NA_1 || tid == C_CS_NA_1 ||
             tid == C_RD_NA_1 || tid == M_EI_NA_1) {
        fire_log(c, LOG_INFO, "System ASDU %s COT=%d",
                 TypeID_toString(tid), CS101_ASDU_getCOT(asdu));
    }
    /* ---- Command ACT_CON ---- */
    else if ((tid >= C_SC_NA_1 && tid <= C_BO_NA_1) ||
             (tid >= C_SC_TA_1 && tid <= C_BO_TA_1)) {
        fire_log(c, LOG_COMMAND, "ACT_CON %s COT=%d",
                 TypeID_toString(tid), CS101_ASDU_getCOT(asdu));
    }
    else {
        fire_log(c, LOG_INFO, "Unhandled ASDU type %d (%s)",
                 tid, TypeID_toString(tid));
    }

    return true;
}

/* ============================================================================
 * Worker Thread
 * ========================================================================= */
#ifdef _WIN32
static DWORD WINAPI conn_thread_fn(LPVOID param)
#else
static void* conn_thread_fn(void *param)
#endif
{
    IEC104_Connection *c = (IEC104_Connection *)param;
    c->running = true;

    while (c->running && !c->stop_flag) {
        fire_log(c, LOG_INFO, "Connecting to %s:%d …",
                 c->config.server_ip, c->config.server_port);
        fire_state(c, CONN_CONNECTING);

        c->connection = CS104_Connection_create(
            c->config.server_ip, c->config.server_port);
        if (!c->connection) {
            fire_log(c, LOG_ERROR, "Cannot create connection object");
            fire_state(c, CONN_ERROR);
            goto reconnect;
        }

        /* APCI parameters (t0-t3, k, w) */
        CS104_APCIParameters apci =
            CS104_Connection_getAPCIParameters(c->connection);
        if (apci) {
            apci->t0 = 10;  apci->t1 = 15;
            apci->t2 = 10;  apci->t3 = 20;
            apci->k  = 12;  apci->w  = 8;
        }

        CS104_Connection_setConnectionHandler(c->connection,
                                              connectionHandler, c);
        CS104_Connection_setASDUReceivedHandler(c->connection,
                                                asduReceivedHandler, c);

        if (CS104_Connection_connect(c->connection)) {
            fire_log(c, LOG_SUCCESS, "Connected!");
            fprintf(stderr, "[CONN] %s: TCP connected to %s:%d\n",
                    c->id, c->config.server_ip, c->config.server_port);
            c->reconnect_count = 0;

            CS104_Connection_sendStartDT(c->connection);
            Thread_sleep(500);
            fprintf(stderr, "[CONN] %s: STARTDT sent, sending GI (CA=%d)\n",
                    c->id, c->config.common_address);

            /* General Interrogation on connect */
            CS104_Connection_sendInterrogationCommand(
                c->connection, CS101_COT_ACTIVATION,
                c->config.common_address, IEC60870_QOI_STATION);

            /* Counter Interrogation on connect (M_IT_NA_1 not covered by GI) */
            CS104_Connection_sendCounterInterrogationCommand(
                c->connection, CS101_COT_ACTIVATION,
                c->config.common_address, IEC60870_QCC_RQT_GENERAL);

            /* Time sync on connect */
            if (c->auto_time_sync) {
                struct sCP56Time2a ts;
                CP56Time2a_createFromMsTimestamp(&ts, Hal_getTimeInMs());
                CS104_Connection_sendClockSyncCommand(
                    c->connection, c->config.common_address, &ts);
            }

            /* Monitor loop */
            uint64_t last_sync = Hal_getTimeInMs();
            while (c->running && !c->stop_flag) {
                Thread_sleep(100);

                /* Periodic time sync */
                if (c->auto_time_sync && c->time_sync_interval_sec > 0) {
                    uint64_t now = Hal_getTimeInMs();
                    uint64_t iv  = (uint64_t)c->time_sync_interval_sec * 1000;
                    if (now - last_sync > iv) {
                        struct sCP56Time2a ts;
                        CP56Time2a_createFromMsTimestamp(&ts, now);
                        CS104_Connection_sendClockSyncCommand(
                            c->connection, c->config.common_address, &ts);
                        last_sync = now;
                        fire_log(c, LOG_INFO, "Auto time sync sent");
                    }
                }

                if (c->state != CONN_CONNECTED) break;
            }
        } else {
            fire_log(c, LOG_ERROR, "Connect failed");
            fire_state(c, CONN_ERROR);
        }

        if (c->connection) {
            CS104_Connection_destroy(c->connection);
            c->connection = NULL;
        }

reconnect:
        if (c->stop_flag || !c->running) break;
        if (!c->auto_reconnect) break;

        c->reconnect_count++;
        if (c->max_reconnect_attempts > 0 &&
            c->reconnect_count >= c->max_reconnect_attempts) {
            fire_log(c, LOG_ERROR, "Max reconnect attempts (%d) reached",
                     c->max_reconnect_attempts);
            break;
        }

        fire_state(c, CONN_RECONNECTING);
        fire_log(c, LOG_RECONNECT, "Retry #%d in %.0fs …",
                 c->reconnect_count, c->reconnect_interval);

        int ms = (int)(c->reconnect_interval * 1000.0f);
        while (ms > 0 && !c->stop_flag) {
            int chunk = (ms > 100) ? 100 : ms;
            Thread_sleep(chunk);
            ms -= chunk;
        }
    }

    c->running = false;
    fire_state(c, CONN_DISCONNECTED);

#ifdef _WIN32
    return 0;
#else
    return NULL;
#endif
}

/* ============================================================================
 * Public API
 * ========================================================================= */
void iec104_conn_init(IEC104_Connection *c, const char *id,
                      const IEC104_StationConfig *cfg)
{
    memset(c, 0, sizeof(*c));
    snprintf(c->id, sizeof(c->id), "%s", id);
    c->config = *cfg;
    c->state  = CONN_DISCONNECTED;
    c->auto_reconnect        = true;
    c->reconnect_interval    = 5.0f;
    c->max_reconnect_attempts = 0; /* unlimited */
    c->auto_time_sync        = true;
    c->time_sync_interval_sec = 3600;
    MTX_INIT(c->mutex);
}

int iec104_conn_start(IEC104_Connection *c)
{
    if (c->running) return -1;
    c->stop_flag = false;
    c->reconnect_count = 0;

#ifdef _WIN32
    c->thread = CreateThread(NULL, 0, conn_thread_fn, c, 0, NULL);
    return (c->thread != NULL) ? 0 : -1;
#else
    return pthread_create(&c->thread, NULL, conn_thread_fn, c);
#endif
}

void iec104_conn_stop(IEC104_Connection *c)
{
    c->stop_flag = true;
    c->running   = false;

#ifdef _WIN32
    if (c->thread) {
        WaitForSingleObject(c->thread, 5000);
        CloseHandle(c->thread);
        c->thread = NULL;
    }
#else
    if (c->thread) {
        pthread_join(c->thread, NULL);
        c->thread = 0;
    }
#endif

    if (c->connection) {
        CS104_Connection_destroy(c->connection);
        c->connection = NULL;
    }
}

void iec104_conn_destroy(IEC104_Connection *c)
{
    iec104_conn_stop(c);
    MTX_DESTROY(c->mutex);
}

bool iec104_conn_is_connected(const IEC104_Connection *c)
{
    return c->state == CONN_CONNECTED;
}

int iec104_conn_send_reset_process(IEC104_Connection *c, uint8_t qrp)
{
    if (!c->connection || c->state != CONN_CONNECTED) return -1;

    /* C_RP_NA_1 (105) — no built-in lib60870 helper, build raw ASDU */
    /* Use default CS104 parameters (IOA=3 bytes, CASDU=2 bytes) */
    struct sCS101_AppLayerParameters defaultParams = {
        .sizeOfTypeId = 1,
        .sizeOfVSQ = 1,
        .sizeOfCOT = 2,
        .originatorAddress = 0,
        .sizeOfCA = 2,
        .sizeOfIOA = 3,
        .maxSizeOfASDU = 249
    };

    CS101_ASDU asdu = CS101_ASDU_create(&defaultParams,
        false, CS101_COT_ACTIVATION, 0, c->config.common_address,
        false, false);

    /* Type ID byte + IOA (3 bytes, all 0) + QRP byte */
    uint8_t payload[4] = {0, 0, 0, qrp};
    CS101_ASDU_addPayload(asdu, payload, 4);

    bool ok = CS104_Connection_sendASDU(c->connection, asdu);
    CS101_ASDU_destroy(asdu);
    return ok ? 0 : -1;
}

int iec104_conn_send_delay_acquisition(IEC104_Connection *c, uint16_t delay_ms)
{
    if (!c->connection || c->state != CONN_CONNECTED) return -1;

    /* C_CD_NA_1 (106) — no built-in lib60870 helper, build raw ASDU */
    struct sCS101_AppLayerParameters defaultParams = {
        .sizeOfTypeId = 1,
        .sizeOfVSQ = 1,
        .sizeOfCOT = 2,
        .originatorAddress = 0,
        .sizeOfCA = 2,
        .sizeOfIOA = 3,
        .maxSizeOfASDU = 249
    };

    CS101_ASDU asdu = CS101_ASDU_create(&defaultParams,
        false, CS101_COT_ACTIVATION, 0, c->config.common_address,
        false, false);

    /* IOA (3 bytes, all 0) + CP16Time2a (2 bytes, little-endian ms) */
    uint8_t payload[5] = {0, 0, 0,
                          (uint8_t)(delay_ms & 0xFF),
                          (uint8_t)(delay_ms >> 8)};
    CS101_ASDU_addPayload(asdu, payload, 5);

    bool ok = CS104_Connection_sendASDU(c->connection, asdu);
    CS101_ASDU_destroy(asdu);
    return ok ? 0 : -1;
}

int iec104_conn_send_gi(IEC104_Connection *c)
{
    if (!c->connection || c->state != CONN_CONNECTED) return -1;
    return CS104_Connection_sendInterrogationCommand(
        c->connection, CS101_COT_ACTIVATION,
        c->config.common_address, IEC60870_QOI_STATION) ? 0 : -1;
}

int iec104_conn_send_clock_sync(IEC104_Connection *c)
{
    if (!c->connection || c->state != CONN_CONNECTED) return -1;
    struct sCP56Time2a ts;
    CP56Time2a_createFromMsTimestamp(&ts, Hal_getTimeInMs());
    return CS104_Connection_sendClockSyncCommand(
        c->connection, c->config.common_address, &ts) ? 0 : -1;
}

int iec104_conn_send_counter_interrogation(IEC104_Connection *c)
{
    if (!c->connection || c->state != CONN_CONNECTED) return -1;
    return CS104_Connection_sendCounterInterrogationCommand(
        c->connection, CS101_COT_ACTIVATION,
        c->config.common_address, 5) ? 0 : -1;
}

int iec104_conn_send_read(IEC104_Connection *c, int ioa)
{
    if (!c->connection || c->state != CONN_CONNECTED) return -1;
    return CS104_Connection_sendReadCommand(
        c->connection, c->config.common_address, ioa) ? 0 : -1;
}

int iec104_conn_send_test(IEC104_Connection *c)
{
    if (!c->connection || c->state != CONN_CONNECTED) return -1;
    return CS104_Connection_sendTestCommand(
        c->connection, c->config.common_address) ? 0 : -1;
}
