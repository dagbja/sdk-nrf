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

#ifdef __cplusplus
}
#endif

#endif // LWM2M_DATA_MODEM_H__

/**@} */
