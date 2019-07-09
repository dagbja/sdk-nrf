/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef LWM2M_GSMA_OBJECTS_H__
#define LWM2M_GSMA_OBJECTS_H__

#include <stdint.h>
#include <stdbool.h>
#include <lwm2m_api.h>

#ifdef __cplusplus
extern "C" {
#endif

/* LWM2M GSMA object IDs. */
#define LWM2M_GSMA_OBJ_POWERUP_LOG                   3351
#define LWM2M_GSMA_OBJ_PLMN_SEARCH_EVENT             3352
#define LWM2M_GSMA_OBJ_SCELL_ID                      3353
#define LWM2M_GSMA_OBJ_CELL_RESELECTION_EVENT        3354
#define LWM2M_GSMA_OBJ_HANDOVER_EVENT                3355
#define LWM2M_GSMA_OBJ_RADIO_LINK_FAILURE_EVENT      3356
#define LWM2M_GSMA_OBJ_RRC_STATE_CHANGE_EVENT        3357
#define LWM2M_GSMA_OBJ_RRC_TIMER_EXPIRY_EVENT        3358
#define LWM2M_GSMA_OBJ_CELL_BLACKLIST_EVENT          3359
#define LWM2M_GSMA_OBJ_ESM_CONTEXT_INFO              3360
#define LWM2M_GSMA_OBJ_EMM_STATE_VALUE               3361
#define LWM2M_GSMA_OBJ_NAS_EMM_TIMER_EXPIRY_EVENT    3362
#define LWM2M_GSMA_OBJ_NAS_ESM_EXPIRY_EVENT          3363
#define LWM2M_GSMA_OBJ_EMM_FAILURE_CAUSE_EVENT       3364
#define LWM2M_GSMA_OBJ_RACH_LATENCY_DELAY            3365
#define LWM2M_GSMA_OBJ_MAC_RACH_ATTEMPT_EVENT        3366
#define LWM2M_GSMA_OBJ_MAC_RACH_ATTEMPT_REASON_EVENT 3367
#define LWM2M_GSMA_OBJ_MAC_TIMER_STATUS_EVENT        3368
#define LWM2M_GSMA_OBJ_MAC_TIMING_ADVANCE_EVENT      3369
#define LWM2M_GSMA_OBJ_SERVING_CELL_MEASUREMENT      3370
#define LWM2M_GSMA_OBJ_NEIGHBOR_CELL_MEASUREMENTS    3371
#define LWM2M_GSMA_OBJ_TIMING_ADVANCE                3372
#define LWM2M_GSMA_OBJ_TX_POWER_HEADROOM_EVENT       3373
#define LWM2M_GSMA_OBJ_RADIO_LINK_MONITORING         3374
#define LWM2M_GSMA_OBJ_PAGING_DRX                    3375
#define LWM2M_GSMA_OBJ_TX_POWER_BACK_OFF_EVENT       3376
#define LWM2M_GSMA_OBJ_MESSAGE_3_REPORT              3377
#define LWM2M_GSMA_OBJ_PBCH_DECODING_RESULTS         3378
#define LWM2M_GSMA_OBJ_PUCCH_POWER_CONTROL           3379
#define LWM2M_GSMA_OBJ_PRACH_REPORT                  3380

/* LWM2M GSMA powerupLog (3351) object resources. */
#define LWM2M_GSMA_POWERUP_LOG_DEVICE_NAME  0
#define LWM2M_GSMA_POWERUP_LOG_TOOL_VERSION 1
#define LWM2M_GSMA_POWERUP_LOG_IMEI         2
#define LWM2M_GSMA_POWERUP_LOG_IMSI         3
#define LWM2M_GSMA_POWERUP_LOG_MSISDN       4

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[5];
    uint16_t         resource_ids[5];

    lwm2m_string_t   device_name;
    lwm2m_string_t   tool_version;
    lwm2m_string_t   imei;
    lwm2m_string_t   imsi;
    lwm2m_string_t   msisdn;
} lwm2m_gsma_powerup_log_t;

/* LWM2M GSMA plmnSearchEvent (3352) object resources. */
#define LWM2M_GSMA_PLMN_SEARCH_EVENT_TIME_SCAN_START 0
#define LWM2M_GSMA_PLMN_SEARCH_EVENT_PLMN_ID         6030
#define LWM2M_GSMA_PLMN_SEARCH_EVENT_BAND_INDICATOR  6031
#define LWM2M_GSMA_PLMN_SEARCH_EVENT_DL_EARFCN       6032

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[4];
    uint16_t         resource_ids[4];

    int32_t          time_scan_start;
    int32_t          plmn_id;
    int32_t          band_indicator;
    int32_t          dl_earfcn;
} lwm2m_gsma_plmn_search_event_t;

/* LWM2M GSMA scellID (3353) object resources. */
#define LWM2M_GSMA_SCELL_ID_PLMN_ID            6030
#define LWM2M_GSMA_SCELL_ID_BAND_INDICATOR     6031
#define LWM2M_GSMA_SCELL_ID_TRACKING_AREA_CODE 2
#define LWM2M_GSMA_SCELL_ID_CELL_ID            6033

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[4];
    uint16_t         resource_ids[4];

    int32_t          plmn_id;
    int32_t          band_indicator;
    int32_t          tracking_area_code;
    int32_t          cell_id;
} lwm2m_gsma_scell_id_t;

/* LWM2M GSMA cellReselectionEvent (3354) object resources. */
#define LWM2M_GSMA_CELL_RESELECTION_EVENT_TIME_RESELECTION_START 0
#define LWM2M_GSMA_CELL_RESELECTION_EVENT_DL_EARFCN              6032
#define LWM2M_GSMA_CELL_RESELECTION_EVENT_CELL_ID                6033
#define LWM2M_GSMA_CELL_RESELECTION_EVENT_FAILURE_TYPE           3

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[4];
    uint16_t         resource_ids[4];

    int32_t          time_reselection_start;
    int32_t          dl_earfcn;
    int32_t          cell_id;
    int32_t          failure_type;
} lwm2m_gsma_cell_reselection_event_t;

/* LWM2M GSMA handoverEvent (3355) object resources. */
#define LWM2M_GSMA_HANDOVER_EVENT_TIME_HANDOVER_START     0
#define LWM2M_GSMA_HANDOVER_EVENT_DL_EARFCN               6032
#define LWM2M_GSMA_HANDOVER_EVENT_CELL_ID                 6033
#define LWM2M_GSMA_HANDOVER_EVENT_HANDOVER_RESULT         3
#define LWM2M_GSMA_HANDOVER_EVENT_TARGET_EARFCN           4
#define LWM2M_GSMA_HANDOVER_EVENT_TARGET_PHYSICAL_CELL_ID 5
#define LWM2M_GSMA_HANDOVER_EVENT_TARGET_CELL_RSRP        6

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[7];
    uint16_t         resource_ids[7];

    int32_t          time_handover_start;
    int32_t          dl_earfcn;
    int32_t          cell_id;
    int32_t          handover_result;
    int32_t          target_earfcn;
    int32_t          target_physical_cell_id;
    int32_t          target_cell_rsrp;
} lwm2m_gsma_handover_event_t;

/* LWM2M GSMA radioLinkFailureEvent (3356) object resources. */
#define LWM2M_GSMA_RADIO_LINK_FAILURE_EVENT_TIME_RLF  0
#define LWM2M_GSMA_RADIO_LINK_FAILURE_EVENT_RLF_CAUSE 1

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[2];
    uint16_t         resource_ids[2];

    int32_t          time_rlf;
    int32_t          rlf_cause;
} lwm2m_gsma_radio_link_failure_event_t;

/* LWM2M GSMA rrcStateChangeEvent (3357) object resources. */
#define LWM2M_GSMA_RRC_STATE_CHANGE_EVENT_RCC_STATE              0
#define LWM2M_GSMA_RRC_STATE_CHANGE_EVENT_RCC_STATE_CHANGE_CAUSE 1

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[2];
    uint16_t         resource_ids[2];

    int32_t          rcc_state;
    int32_t          rcc_state_change_cause;
} lwm2m_gsma_rrc_state_change_event_t;

/* LWM2M GSMA rrcTimerExpiryEvent (3358) object resources. */
#define LWM2M_GSMA_RRC_TIMER_EXPIRY_EVENT_RRC_TIMER_EXPIRY_EVENT 0

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[1];
    uint16_t         resource_ids[1];

    int32_t          rrc_timer_expiry_event;
} lwm2m_gsma_rrc_timer_expiry_event_t;

/* LWM2M GSMA cellBlacklistEvent (3359) object resources. */
#define LWM2M_GSMA_CELL_BLACKLIST_EVENT_DL_EARFCN 6032
#define LWM2M_GSMA_CELL_BLACKLIST_EVENT_CELL_ID   6033

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[2];
    uint16_t         resource_ids[2];

    int32_t          dl_earfcn;
    int32_t          cell_id;
} lwm2m_gsma_cell_blacklist_event_t;

/* LWM2M GSMA esmContextInfo (3360) object resources. */
#define LWM2M_GSMA_ESM_CONTEXT_INFO_CONTEXT_TYPE    0
#define LWM2M_GSMA_ESM_CONTEXT_INFO_BEARER_STATE    1
#define LWM2M_GSMA_ESM_CONTEXT_INFO_RADIO_BEARER_ID 2
#define LWM2M_GSMA_ESM_CONTEXT_INFO_QCI             3

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[4];
    uint16_t         resource_ids[4];

    int32_t          context_type;
    int32_t          bearer_state;
    int32_t          radio_bearer_id;
    int32_t          qci;
} lwm2m_gsma_esm_context_info_t;

/* LWM2M GSMA emmStateValue (3361) object resources. */
#define LWM2M_GSMA_EMM_STATE_VALUE_EMM_STATE    0
#define LWM2M_GSMA_EMM_STATE_VALUE_EMM_SUBSTATE 1

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[2];
    uint16_t         resource_ids[2];

    int32_t          emm_state;
    int32_t          emm_substate;
} lwm2m_gsma_emm_state_value_t;

/* LWM2M GSMA nasEmmTimerExpiryEvent (3362) object resources. */
#define LWM2M_GSMA_NAS_EMM_TIMER_EXPIRY_EVENT_NAS_EMM_TIMER_EXPIRY_EVENT 0

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[1];
    uint16_t         resource_ids[1];

    int32_t          nas_emm_timer_expiry_event;
} lwm2m_gsma_nas_emm_timer_expiry_event_t;

/* LWM2M GSMA nasEsmExpiryEvent (3363) object resources. */
#define LWM2M_GSMA_NAS_ESM_EXPIRY_EVENT_NAS_ESM_EXPIRY_EVENT 0

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[1];
    uint16_t         resource_ids[1];

    int32_t          nas_esm_expiry_event;
} lwm2m_gsma_nas_esm_expiry_event_t;

/* LWM2M GSMA emmFailureCauseEvent (3364) object resources. */
#define LWM2M_GSMA_EMM_FAILURE_CAUSE_EVENT_EMM_CAUSE 0

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[1];
    uint16_t         resource_ids[1];

    int32_t          emm_cause;
} lwm2m_gsma_emm_failure_cause_event_t;

/* LWM2M GSMA rachLatency_delay (3365) object resources. */
#define LWM2M_GSMA_RACH_LATENCY_DELAY_SYS_FRAME_NUMBER 6037
#define LWM2M_GSMA_RACH_LATENCY_DELAY_SUB_FRAME_NUMBER 6038
#define LWM2M_GSMA_RACH_LATENCY_DELAY_RACH_LATENCY_VAL 2
#define LWM2M_GSMA_RACH_LATENCY_DELAY_DELAY            3

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[4];
    uint16_t         resource_ids[4];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          rach_latency_val;
    int32_t          delay;
} lwm2m_gsma_rach_latency_delay_t;

/* LWM2M GSMA macRachAttemptEvent (3366) object resources. */
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_RACH_ATTEMPT_COUNTER        0
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_MAC_RACH_ATTEMPT_EVENT_TYPE 1
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_CONTENTION_BASED            2
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_RACH_MESSAGE                3
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_PREAMBLE_INDEX              4
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_PREAMBLE_POWER_OFFSET       5
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_BACKOFF_TIME                6
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_MSG_2_RESULT                7
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_TIMING_ADJUSTEMENT_VALUE    8

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[9];
    uint16_t         resource_ids[9];

    int32_t          rach_attempt_counter;
    int32_t          mac_rach_attempt_event_type;
    bool             contention_based;
    int32_t          rach_message;
    int32_t          preamble_index;
    int32_t          preamble_power_offset;
    int32_t          backoff_time;
    bool             msg_2_result;
    int32_t          timing_adjustement_value;
} lwm2m_gsma_mac_rach_attempt_event_t;

/* LWM2M GSMA macRachAttemptReasonEvent (3367) object resources. */
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_MAC_RACH_ATTEMPT_REASON_TYPE 0
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_UE_ID                        1
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_CONTENTION_BASED             2
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_PREAMBLE                     3
#define LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_PREAMBLE_GROUP_CHOSEN        4

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[5];
    uint16_t         resource_ids[5];

    int32_t          mac_rach_attempt_reason_type;
    int32_t          ue_id;
    bool             contention_based;
    int32_t          preamble;
    bool             preamble_group_chosen;
} lwm2m_gsma_mac_rach_attempt_reason_event_t;

/* LWM2M GSMA macTimerStatusEvent (3368) object resources. */
#define LWM2M_GSMA_MAC_TIMER_STATUS_EVENT_MAC_TIMER_NAME 0

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[1];
    uint16_t         resource_ids[1];

    int32_t          mac_timer_name;
} lwm2m_gsma_mac_timer_status_event_t;

/* LWM2M GSMA macTimingAdvanceEvent (3369) object resources. */
#define LWM2M_GSMA_MAC_TIMING_ADVANCE_EVENT_TIMER_VALUE    0
#define LWM2M_GSMA_MAC_TIMING_ADVANCE_EVENT_TIMING_ADVANCE 1

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[2];
    uint16_t         resource_ids[2];

    int32_t          timer_value;
    int32_t          timing_advance;
} lwm2m_gsma_mac_timing_advance_event_t;

/* LWM2M GSMA ServingCellMeasurement (3370) object resources. */
#define LWM2M_GSMA_SERVING_CELL_MEASUREMENT_SYS_FRAME_NUMBER 6037
#define LWM2M_GSMA_SERVING_CELL_MEASUREMENT_SUB_FRAME_NUMBER 6038
#define LWM2M_GSMA_SERVING_CELL_MEASUREMENT_PCI              6034
#define LWM2M_GSMA_SERVING_CELL_MEASUREMENT_RSRP             6035
#define LWM2M_GSMA_SERVING_CELL_MEASUREMENT_RSRQ             6036
#define LWM2M_GSMA_SERVING_CELL_MEASUREMENT_DL_EARFCN        6032

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[6];
    uint16_t         resource_ids[6];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          pci;
    int32_t          rsrp;
    int32_t          rsrq;
    int32_t          dl_earfcn;
} lwm2m_gsma_serving_cell_measurement_t;

/* LWM2M GSMA NeighborCellMeasurements (3371) object resources. */
#define LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_SYS_FRAME_NUMBER 6037
#define LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_SUB_FRAME_NUMBER 6038
#define LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_PCI              6034
#define LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_RSRP             6035
#define LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_RSRQ             6036
#define LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_DL_EARFCN        6032

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[6];
    uint16_t         resource_ids[6];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          pci;
    int32_t          rsrp;
    int32_t          rsrq;
    int32_t          dl_earfcn;
} lwm2m_gsma_neighbor_cell_measurements_t;

/* LWM2M GSMA TimingAdvance (3372) object resources. */
#define LWM2M_GSMA_TIMING_ADVANCE_SYS_FRAME_NUMBER 6037
#define LWM2M_GSMA_TIMING_ADVANCE_SUB_FRAME_NUMBER 6038
#define LWM2M_GSMA_TIMING_ADVANCE_TIMING_ADVANCE   2

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[3];
    uint16_t         resource_ids[3];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          timing_advance;
} lwm2m_gsma_timing_advance_t;

/* LWM2M GSMA txPowerHeadroomEvent (3373) object resources. */
#define LWM2M_GSMA_TX_POWER_HEADROOM_EVENT_SYS_FRAME_NUMBER 6037
#define LWM2M_GSMA_TX_POWER_HEADROOM_EVENT_SUB_FRAME_NUMBER 6038
#define LWM2M_GSMA_TX_POWER_HEADROOM_EVENT_HEADROOM_VALUE   2

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[3];
    uint16_t         resource_ids[3];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          headroom_value;
} lwm2m_gsma_tx_power_headroom_event_t;

/* LWM2M GSMA radioLinkMonitoring (3374) object resources. */
#define LWM2M_GSMA_RADIO_LINK_MONITORING_SYS_FRAME_NUMBER  6037
#define LWM2M_GSMA_RADIO_LINK_MONITORING_SUB_FRAME_NUMBER  6038
#define LWM2M_GSMA_RADIO_LINK_MONITORING_OUT_OF_SYNC_COUNT 2
#define LWM2M_GSMA_RADIO_LINK_MONITORING_IN_SYNC_COUNT     3
#define LWM2M_GSMA_RADIO_LINK_MONITORING_T310_TIMER        4

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[5];
    uint16_t         resource_ids[5];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          out_of_sync_count;
    int32_t          in_sync_count;
    bool             t310_timer;
} lwm2m_gsma_radio_link_monitoring_t;

/* LWM2M GSMA PagingDRX (3375) object resources. */
#define LWM2M_GSMA_PAGING_DRX_DL_EARFCN                6032
#define LWM2M_GSMA_PAGING_DRX_PCI                      6034
#define LWM2M_GSMA_PAGING_DRX_PAGING_CYCLE             2
#define LWM2M_GSMA_PAGING_DRX_DRX_NB                   3
#define LWM2M_GSMA_PAGING_DRX_UEID                     4
#define LWM2M_GSMA_PAGING_DRX_DRX_SYS_FRAME_NUM_OFFSET 5
#define LWM2M_GSMA_PAGING_DRX_DRX_SUB_FRAME_NUM_OFFSET 6

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[7];
    uint16_t         resource_ids[7];

    int32_t          dl_earfcn;
    int32_t          pci;
    int32_t          paging_cycle;
    int32_t          drx_nb;
    int32_t          ueid;
    int32_t          drx_sys_frame_num_offset;
    int32_t          drx_sub_frame_num_offset;
} lwm2m_gsma_paging_drx_t;

/* LWM2M GSMA txPowerBackOffEvent (3376) object resources. */
#define LWM2M_GSMA_TX_POWER_BACK_OFF_EVENT_TX_POWER_BACKOFF 0

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[1];
    uint16_t         resource_ids[1];

    int32_t          tx_power_backoff;
} lwm2m_gsma_tx_power_back_off_event_t;

/* LWM2M GSMA Message3Report (3377) object resources. */
#define LWM2M_GSMA_MESSAGE_3_REPORT_TPC                        0
#define LWM2M_GSMA_MESSAGE_3_REPORT_RESOURCE_INDICATOR_VALUE   1
#define LWM2M_GSMA_MESSAGE_3_REPORT_CQI                        2
#define LWM2M_GSMA_MESSAGE_3_REPORT_UPLINK_DELAY               3
#define LWM2M_GSMA_MESSAGE_3_REPORT_HOPPING_ENABLED            4
#define LWM2M_GSMA_MESSAGE_3_REPORT_NUM_RB                     5
#define LWM2M_GSMA_MESSAGE_3_REPORT_TRANSPORT_BLOCK_SIZE_INDEX 6
#define LWM2M_GSMA_MESSAGE_3_REPORT_MODULATION_TYPE            7
#define LWM2M_GSMA_MESSAGE_3_REPORT_REDUNDANCY_VERSION_INDEX   8

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[9];
    uint16_t         resource_ids[9];

    int32_t          tpc;
    int32_t          resource_indicator_value;
    int32_t          cqi;
    bool             uplink_delay;
    bool             hopping_enabled;
    int32_t          num_rb;
    int32_t          transport_block_size_index;
    int32_t          modulation_type;
    int32_t          redundancy_version_index;
} lwm2m_gsma_message_3_report_t;

/* LWM2M GSMA PbchDecodingResults (3378) object resources. */
#define LWM2M_GSMA_PBCH_DECODING_RESULTS_SERVING_CELL_ID  0
#define LWM2M_GSMA_PBCH_DECODING_RESULTS_CRC_RESULT       1
#define LWM2M_GSMA_PBCH_DECODING_RESULTS_SYS_FRAME_NUMBER 6037

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[3];
    uint16_t         resource_ids[3];

    int32_t          serving_cell_id;
    bool             crc_result;
    int32_t          sys_frame_number;
} lwm2m_gsma_pbch_decoding_results_t;

/* LWM2M GSMA pucchPowerControl (3379) object resources. */
#define LWM2M_GSMA_PUCCH_POWER_CONTROL_SYS_FRAME_NUMBER     6037
#define LWM2M_GSMA_PUCCH_POWER_CONTROL_SUB_FRAME_NUMBER     6038
#define LWM2M_GSMA_PUCCH_POWER_CONTROL_PUCCH_TX_POWER_VALUE 2
#define LWM2M_GSMA_PUCCH_POWER_CONTROL_DL_PATH_LOSS         3

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[4];
    uint16_t         resource_ids[4];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          pucch_tx_power_value;
    int32_t          dl_path_loss;
} lwm2m_gsma_pucch_power_control_t;

/* LWM2M GSMA PrachReport (3380) object resources. */
#define LWM2M_GSMA_PRACH_REPORT_SYS_FRAME_NUMBER        6037
#define LWM2M_GSMA_PRACH_REPORT_SUB_FRAME_NUMBER        6038
#define LWM2M_GSMA_PRACH_REPORT_RACH_TX_POWER           2
#define LWM2M_GSMA_PRACH_REPORT_ZAD_OFF_SEQ_NUM         3
#define LWM2M_GSMA_PRACH_REPORT_PRACH_CONFIG            4
#define LWM2M_GSMA_PRACH_REPORT_PREAMBLE_FORMAT         5
#define LWM2M_GSMA_PRACH_REPORT_MAX_TRANSMISSION_MSG_3  6
#define LWM2M_GSMA_PRACH_REPORT_RA_RESPONSE_WINDOW_SIZE 7
#define LWM2M_GSMA_PRACH_REPORT_RACH_REQUEST_RESULT     8
#define LWM2M_GSMA_PRACH_REPORT_CE_MODE                 9
#define LWM2M_GSMA_PRACH_REPORT_CE_LEVEL                10
#define LWM2M_GSMA_PRACH_REPORT_NUM_PRACH_REPETITION    11
#define LWM2M_GSMA_PRACH_REPORT_PRACH_REPETITION_SEQ    12

typedef struct
{
    lwm2m_instance_t proto;
    uint8_t          operations[13];
    uint16_t         resource_ids[13];

    int32_t          sys_frame_number;
    int32_t          sub_frame_number;
    int32_t          rach_tx_power;
    int32_t          zad_off_seq_num;
    int32_t          prach_config;
    int32_t          preamble_format;
    int32_t          max_transmission_msg_3;
    int32_t          ra_response_window_size;
    bool             rach_request_result;
    int32_t          ce_mode;
    int32_t          ce_level;
    int32_t          num_prach_repetition;
    int32_t          prach_repetition_seq;
} lwm2m_gsma_prach_report_t;

#ifdef __cplusplus
}
#endif

#endif // LWM2M_GSMA_OBJECTS_H__
