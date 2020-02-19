/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "lwm2m_gsma_objects.h"

#define LWM2M_RESOURCE_COUNT(type) \
    (sizeof(((type *)0)->resource_ids) / sizeof(((type *)0)->resource_ids[0]))

#define LWM2M_INSTANCE_INIT(p_instance, type) \
    memset(p_instance, 0, sizeof(type)); \
    p_instance->proto.operations_offset   = offsetof(type, operations); \
    p_instance->proto.resource_ids_offset = offsetof(type, resource_ids); \
    p_instance->proto.num_resources       = LWM2M_RESOURCE_COUNT(type);

void lwm2m_gsma_instance_powerup_log_init(lwm2m_gsma_powerup_log_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_powerup_log_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_POWERUP_LOG;

    p_instance->resource_ids[0] = LWM2M_GSMA_POWERUP_LOG_DEVICE_NAME;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_POWERUP_LOG_TOOL_VERSION;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_POWERUP_LOG_IMEI;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_POWERUP_LOG_IMSI;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_POWERUP_LOG_MSISDN;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_plmn_search_event_init(lwm2m_gsma_plmn_search_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_plmn_search_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_PLMN_SEARCH_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_PLMN_SEARCH_EVENT_TIME_SCAN_START;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_PLMN_SEARCH_EVENT_PLMN_ID;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_PLMN_SEARCH_EVENT_BAND_INDICATOR;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_PLMN_SEARCH_EVENT_DL_EARFCN;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_scell_id_init(lwm2m_gsma_scell_id_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_scell_id_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_SCELL_ID;

    p_instance->resource_ids[0] = LWM2M_GSMA_SCELL_ID_PLMN_ID;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_SCELL_ID_BAND_INDICATOR;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_SCELL_ID_TRACKING_AREA_CODE;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_SCELL_ID_CELL_ID;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_cell_reselection_event_init(lwm2m_gsma_cell_reselection_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_cell_reselection_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_CELL_RESELECTION_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_CELL_RESELECTION_EVENT_TIME_RESELECTION_START;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_CELL_RESELECTION_EVENT_DL_EARFCN;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_CELL_RESELECTION_EVENT_CELL_ID;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_CELL_RESELECTION_EVENT_FAILURE_TYPE;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_handover_event_init(lwm2m_gsma_handover_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_handover_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_HANDOVER_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_HANDOVER_EVENT_TIME_HANDOVER_START;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_HANDOVER_EVENT_DL_EARFCN;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_HANDOVER_EVENT_CELL_ID;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_HANDOVER_EVENT_HANDOVER_RESULT;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_HANDOVER_EVENT_TARGET_EARFCN;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[5] = LWM2M_GSMA_HANDOVER_EVENT_TARGET_PHYSICAL_CELL_ID;
    p_instance->operations[5]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[6] = LWM2M_GSMA_HANDOVER_EVENT_TARGET_CELL_RSRP;
    p_instance->operations[6]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_radio_link_failure_event_init(lwm2m_gsma_radio_link_failure_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_radio_link_failure_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_RADIO_LINK_FAILURE_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_RADIO_LINK_FAILURE_EVENT_TIME_RLF;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_RADIO_LINK_FAILURE_EVENT_RLF_CAUSE;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_rrc_state_change_event_init(lwm2m_gsma_rrc_state_change_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_rrc_state_change_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_RRC_STATE_CHANGE_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_RRC_STATE_CHANGE_EVENT_RCC_STATE;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_RRC_STATE_CHANGE_EVENT_RCC_STATE_CHANGE_CAUSE;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_rrc_timer_expiry_event_init(lwm2m_gsma_rrc_timer_expiry_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_rrc_timer_expiry_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_RRC_TIMER_EXPIRY_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_RRC_TIMER_EXPIRY_EVENT_RRC_TIMER_EXPIRY_EVENT;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_cell_blacklist_event_init(lwm2m_gsma_cell_blacklist_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_cell_blacklist_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_CELL_BLACKLIST_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_CELL_BLACKLIST_EVENT_DL_EARFCN;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_CELL_BLACKLIST_EVENT_CELL_ID;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_esm_context_info_init(lwm2m_gsma_esm_context_info_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_esm_context_info_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_ESM_CONTEXT_INFO;

    p_instance->resource_ids[0] = LWM2M_GSMA_ESM_CONTEXT_INFO_CONTEXT_TYPE;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_ESM_CONTEXT_INFO_BEARER_STATE;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_ESM_CONTEXT_INFO_RADIO_BEARER_ID;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_ESM_CONTEXT_INFO_QCI;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_emm_state_value_init(lwm2m_gsma_emm_state_value_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_emm_state_value_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_EMM_STATE_VALUE;

    p_instance->resource_ids[0] = LWM2M_GSMA_EMM_STATE_VALUE_EMM_STATE;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_EMM_STATE_VALUE_EMM_SUBSTATE;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_nas_emm_timer_expiry_event_init(lwm2m_gsma_nas_emm_timer_expiry_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_nas_emm_timer_expiry_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_NAS_EMM_TIMER_EXPIRY_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_NAS_EMM_TIMER_EXPIRY_EVENT_NAS_EMM_TIMER_EXPIRY_EVENT;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_nas_esm_expiry_event_init(lwm2m_gsma_nas_esm_expiry_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_nas_esm_expiry_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_NAS_ESM_EXPIRY_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_NAS_ESM_EXPIRY_EVENT_NAS_ESM_EXPIRY_EVENT;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_emm_failure_cause_event_init(lwm2m_gsma_emm_failure_cause_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_emm_failure_cause_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_EMM_FAILURE_CAUSE_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_EMM_FAILURE_CAUSE_EVENT_EMM_CAUSE;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_rach_latency_delay_init(lwm2m_gsma_rach_latency_delay_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_rach_latency_delay_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_RACH_LATENCY_DELAY;

    p_instance->resource_ids[0] = LWM2M_GSMA_RACH_LATENCY_DELAY_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_RACH_LATENCY_DELAY_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_RACH_LATENCY_DELAY_RACH_LATENCY_VAL;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_RACH_LATENCY_DELAY_DELAY;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_mac_rach_attempt_event_init(lwm2m_gsma_mac_rach_attempt_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_mac_rach_attempt_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_MAC_RACH_ATTEMPT_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_RACH_ATTEMPT_COUNTER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_MAC_RACH_ATTEMPT_EVENT_TYPE;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_CONTENTION_BASED;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_RACH_MESSAGE;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_PREAMBLE_INDEX;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[5] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_PREAMBLE_POWER_OFFSET;
    p_instance->operations[5]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[6] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_BACKOFF_TIME;
    p_instance->operations[6]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[7] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_MSG_2_RESULT;
    p_instance->operations[7]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[8] = LWM2M_GSMA_MAC_RACH_ATTEMPT_EVENT_TIMING_ADJUSTEMENT_VALUE;
    p_instance->operations[8]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_mac_rach_attempt_reason_event_init(lwm2m_gsma_mac_rach_attempt_reason_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_mac_rach_attempt_reason_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_MAC_RACH_ATTEMPT_REASON_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_MAC_RACH_ATTEMPT_REASON_TYPE;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_UE_ID;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_CONTENTION_BASED;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_PREAMBLE;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_MAC_RACH_ATTEMPT_REASON_EVENT_PREAMBLE_GROUP_CHOSEN;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_mac_timer_status_event_init(lwm2m_gsma_mac_timer_status_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_mac_timer_status_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_MAC_TIMER_STATUS_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_MAC_TIMER_STATUS_EVENT_MAC_TIMER_NAME;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_mac_timing_advance_event_init(lwm2m_gsma_mac_timing_advance_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_mac_timing_advance_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_MAC_TIMING_ADVANCE_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_MAC_TIMING_ADVANCE_EVENT_TIMER_VALUE;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_MAC_TIMING_ADVANCE_EVENT_TIMING_ADVANCE;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_serving_cell_measurement_init(lwm2m_gsma_serving_cell_measurement_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_serving_cell_measurement_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_SERVING_CELL_MEASUREMENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_SERVING_CELL_MEASUREMENT_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_SERVING_CELL_MEASUREMENT_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_SERVING_CELL_MEASUREMENT_PCI;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_SERVING_CELL_MEASUREMENT_RSRP;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_SERVING_CELL_MEASUREMENT_RSRQ;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[5] = LWM2M_GSMA_SERVING_CELL_MEASUREMENT_DL_EARFCN;
    p_instance->operations[5]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_neighbor_cell_measurements_init(lwm2m_gsma_neighbor_cell_measurements_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_neighbor_cell_measurements_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_NEIGHBOR_CELL_MEASUREMENTS;

    p_instance->resource_ids[0] = LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_PCI;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_RSRP;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_RSRQ;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[5] = LWM2M_GSMA_NEIGHBOR_CELL_MEASUREMENTS_DL_EARFCN;
    p_instance->operations[5]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_timing_advance_init(lwm2m_gsma_timing_advance_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_timing_advance_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_TIMING_ADVANCE;

    p_instance->resource_ids[0] = LWM2M_GSMA_TIMING_ADVANCE_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_TIMING_ADVANCE_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_TIMING_ADVANCE_TIMING_ADVANCE;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_tx_power_headroom_event_init(lwm2m_gsma_tx_power_headroom_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_tx_power_headroom_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_TX_POWER_HEADROOM_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_TX_POWER_HEADROOM_EVENT_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_TX_POWER_HEADROOM_EVENT_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_TX_POWER_HEADROOM_EVENT_HEADROOM_VALUE;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_radio_link_monitoring_init(lwm2m_gsma_radio_link_monitoring_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_radio_link_monitoring_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_RADIO_LINK_MONITORING;

    p_instance->resource_ids[0] = LWM2M_GSMA_RADIO_LINK_MONITORING_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_RADIO_LINK_MONITORING_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_RADIO_LINK_MONITORING_OUT_OF_SYNC_COUNT;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_RADIO_LINK_MONITORING_IN_SYNC_COUNT;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_RADIO_LINK_MONITORING_T310_TIMER;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_paging_drx_init(lwm2m_gsma_paging_drx_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_paging_drx_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_PAGING_DRX;

    p_instance->resource_ids[0] = LWM2M_GSMA_PAGING_DRX_DL_EARFCN;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_PAGING_DRX_PCI;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_PAGING_DRX_PAGING_CYCLE;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_PAGING_DRX_DRX_NB;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_PAGING_DRX_UEID;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[5] = LWM2M_GSMA_PAGING_DRX_DRX_SYS_FRAME_NUM_OFFSET;
    p_instance->operations[5]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[6] = LWM2M_GSMA_PAGING_DRX_DRX_SUB_FRAME_NUM_OFFSET;
    p_instance->operations[6]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_tx_power_back_off_event_init(lwm2m_gsma_tx_power_back_off_event_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_tx_power_back_off_event_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_TX_POWER_BACK_OFF_EVENT;

    p_instance->resource_ids[0] = LWM2M_GSMA_TX_POWER_BACK_OFF_EVENT_TX_POWER_BACKOFF;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_message_3_report_init(lwm2m_gsma_message_3_report_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_message_3_report_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_MESSAGE_3_REPORT;

    p_instance->resource_ids[0] = LWM2M_GSMA_MESSAGE_3_REPORT_TPC;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_MESSAGE_3_REPORT_RESOURCE_INDICATOR_VALUE;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_MESSAGE_3_REPORT_CQI;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_MESSAGE_3_REPORT_UPLINK_DELAY;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_MESSAGE_3_REPORT_HOPPING_ENABLED;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[5] = LWM2M_GSMA_MESSAGE_3_REPORT_NUM_RB;
    p_instance->operations[5]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[6] = LWM2M_GSMA_MESSAGE_3_REPORT_TRANSPORT_BLOCK_SIZE_INDEX;
    p_instance->operations[6]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[7] = LWM2M_GSMA_MESSAGE_3_REPORT_MODULATION_TYPE;
    p_instance->operations[7]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[8] = LWM2M_GSMA_MESSAGE_3_REPORT_REDUNDANCY_VERSION_INDEX;
    p_instance->operations[8]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_pbch_decoding_results_init(lwm2m_gsma_pbch_decoding_results_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_pbch_decoding_results_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_PBCH_DECODING_RESULTS;

    p_instance->resource_ids[0] = LWM2M_GSMA_PBCH_DECODING_RESULTS_SERVING_CELL_ID;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_PBCH_DECODING_RESULTS_CRC_RESULT;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_PBCH_DECODING_RESULTS_SYS_FRAME_NUMBER;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_pucch_power_control_init(lwm2m_gsma_pucch_power_control_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_pucch_power_control_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_PUCCH_POWER_CONTROL;

    p_instance->resource_ids[0] = LWM2M_GSMA_PUCCH_POWER_CONTROL_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_PUCCH_POWER_CONTROL_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_PUCCH_POWER_CONTROL_PUCCH_TX_POWER_VALUE;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_PUCCH_POWER_CONTROL_DL_PATH_LOSS;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;
}

void lwm2m_gsma_instance_prach_report_init(lwm2m_gsma_prach_report_t * p_instance)
{
    LWM2M_INSTANCE_INIT(p_instance, lwm2m_gsma_prach_report_t)

    p_instance->proto.object_id = LWM2M_GSMA_OBJ_PRACH_REPORT;

    p_instance->resource_ids[0] = LWM2M_GSMA_PRACH_REPORT_SYS_FRAME_NUMBER;
    p_instance->operations[0]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[1] = LWM2M_GSMA_PRACH_REPORT_SUB_FRAME_NUMBER;
    p_instance->operations[1]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[2] = LWM2M_GSMA_PRACH_REPORT_RACH_TX_POWER;
    p_instance->operations[2]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[3] = LWM2M_GSMA_PRACH_REPORT_ZAD_OFF_SEQ_NUM;
    p_instance->operations[3]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[4] = LWM2M_GSMA_PRACH_REPORT_PRACH_CONFIG;
    p_instance->operations[4]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[5] = LWM2M_GSMA_PRACH_REPORT_PREAMBLE_FORMAT;
    p_instance->operations[5]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[6] = LWM2M_GSMA_PRACH_REPORT_MAX_TRANSMISSION_MSG_3;
    p_instance->operations[6]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[7] = LWM2M_GSMA_PRACH_REPORT_RA_RESPONSE_WINDOW_SIZE;
    p_instance->operations[7]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[8] = LWM2M_GSMA_PRACH_REPORT_RACH_REQUEST_RESULT;
    p_instance->operations[8]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[9] = LWM2M_GSMA_PRACH_REPORT_CE_MODE;
    p_instance->operations[9]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[10] = LWM2M_GSMA_PRACH_REPORT_CE_LEVEL;
    p_instance->operations[10]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[11] = LWM2M_GSMA_PRACH_REPORT_NUM_PRACH_REPETITION;
    p_instance->operations[11]   = LWM2M_OPERATION_CODE_READ;

    p_instance->resource_ids[12] = LWM2M_GSMA_PRACH_REPORT_PRACH_REPETITION_SEQ;
    p_instance->operations[12]   = LWM2M_OPERATION_CODE_READ;
}
