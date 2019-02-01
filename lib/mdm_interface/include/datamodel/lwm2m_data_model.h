/*$$$LICENCE_NORDIC_STANDARD<2019>$$$*/
/**@file lwm2m_data_model.h
 *
 * @brief Data model of objects given by the Modem.
 * @{
 */
#ifndef LWM2M_DATA_MODEM_H__
#define LWM2M_DATA_MODEM_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t rxlev;
    uint8_t ber;
    uint8_t rscp;
    uint8_t ecno;
    uint8_t rsrq;
    uint8_t rsrp;
} lwm2m_model_cesq_rsp_t;

typedef struct
{
    uint8_t n;
    uint8_t stat;
    char * tac;
    char * ci;
    uint8_t act;
    uint8_t cause_type;
    uint8_t reject_cause;
    uint8_t active_time;
    uint8_t periodic_tau;
} lwm2m_model_cereg_rsp_t;

typedef struct
{
    uint8_t mode;
    uint8_t format;
    char * oper;
    uint8_t act;
} lwm2m_model_cops_rsp_t;

typedef struct
{
    char * numberx;
    uint8_t typex;
} lwm2m_model_cnum_rsp_t;

typedef struct
{
    char * manufacturer;
} lwm2m_model_cgmi_rsp_t;

typedef struct
{
    char * model;
} lwm2m_model_cgmm_rsp_t;

typedef struct
{
    char * revision;
} lwm2m_model_cgmr_rsp_t;

typedef struct
{
    char * serial; // Default value is <imei>.
} lwm2m_model_cgsn_rsp_t;

#ifdef __cplusplus
}
#endif

#endif // LWM2M_DATA_MODEM_H__

/**@} */
