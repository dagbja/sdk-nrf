#include <stdint.h>
#include <stddef.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_observer_storage.h>
#include <lwm2m_remote.h>
#include <lwm2m_instance_storage.h>
#include <operator_check.h>

#include <lwm2m_objects.h>
#include <lwm2m_objects_tlv.h>
#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_access_control.h>
#include <lwm2m_apn_conn_prof.h>
#include <lwm2m_portfolio.h>
#include <lwm2m_conn_ext.h>

#include <lwm2m_os.h>


/* LwM2M storage version, used for versioning of records */
#define LWM2M_STORAGE_VERSION 1

#define LWM2M_STORAGE_ID_MISC               (LWM2M_OS_STORAGE_END)
#define LWM2M_STORAGE_ID_SECURITY           (LWM2M_OS_STORAGE_END - 1)
#define LWM2M_STORAGE_ID_SERVER             (LWM2M_OS_STORAGE_END - 2)
#define LWM2M_STORAGE_ID_ACL                (LWM2M_OS_STORAGE_END - 3)
#define LWM2M_STORAGE_ID_LOCATION           (LWM2M_OS_STORAGE_END - 4)
#define LWM2M_STORAGE_ID_MSISDN             (LWM2M_OS_STORAGE_END - 5)
#define LWM2M_STORAGE_ID_DEBUG              (LWM2M_OS_STORAGE_END - 6)
#define LWM2M_MODEM_FIRMWARE_VERSION        (LWM2M_OS_STORAGE_END - 7)
#define LWM2M_MODEM_FIRMWARE_READY          (LWM2M_OS_STORAGE_END - 8)
#define LWM2M_MODEM_FIRMWARE_UPDATE         (LWM2M_OS_STORAGE_END - 9)
#define LWM2M_MODEM_FIRMWARE_URI            (LWM2M_OS_STORAGE_END - 10)
#define LWM2M_STORAGE_OPERATOR_ID           (LWM2M_OS_STORAGE_END - 11)
#define LWM2M_STORAGE_APN_CONN_PROFILE      (LWM2M_OS_STORAGE_END - 12)
#define LWM2M_STORAGE_PORTFOLIO             (LWM2M_OS_STORAGE_END - 13)
#define LWM2M_STORAGE_CONN_EXTENSION        (LWM2M_OS_STORAGE_END - 14)
#define LWM2M_STORED_CLASS3_APN             (LWM2M_OS_STORAGE_END - 15)
#define LWM2M_STORAGE_ID_VERSION            (LWM2M_OS_STORAGE_END - 16)

#define LWM2M_OBSERVERS_BASE                (LWM2M_OS_STORAGE_BASE + 0)
/*
 * The storage range base after LWM2M_OBSERVERS will start at
 * LWM2M_OBSERVERS_BASE + CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS.
 * Make sure that LWM2M_OS_STORAGE_END and LWM2M_OS_STORAGE_BASE range
 * is wide enough to accompany the full range of storage items.
 */
#define LWM2M_NOTIF_ATTR_BASE (LWM2M_OBSERVERS_BASE + \
                               CONFIG_NRF_COAP_OBSERVE_MAX_NUM_OBSERVERS)

static const char *obj_str_get(uint16_t id);
static int lwm2m_storage_version_load(struct lwm2m_storage_version *ver);
static int lwm2m_storage_version_store(void);
static void lwm2m_storage_version_update(uint8_t from_version);

/* Global buffer for encoding LwM2M resource when writing to flash.
 * This is generous at the moment, as it should be.
 */
static uint8_t buf[512];

int32_t lwm2m_instance_storage_init(void)
{
    // NVS subystem is initialized in lwm2m_os_init().

    int err;
    struct lwm2m_storage_version ver;

    lwm2m_observer_storage_set_callbacks(lwm2m_observer_store,
                                         lwm2m_observer_load,
                                         lwm2m_observer_delete);

    lwm2m_notif_attr_storage_set_callbacks(lwm2m_notif_attr_store,
                                           lwm2m_notif_attr_load,
                                           lwm2m_notif_attr_delete);

    err = lwm2m_storage_version_load(&ver);
    if (err || ver.version != LWM2M_STORAGE_VERSION) {
        lwm2m_storage_version_update(err ? 0 : ver.version);
        lwm2m_storage_version_store();
    }

    /* Suppress warning */
    (void) obj_str_get;

    return 0;
}

int32_t lwm2m_instance_storage_deinit(void)
{
    return 0;
}

int32_t lwm2m_storage_misc_data_load(lwm2m_storage_misc_data_t *p_data)
{
    int read;

    read = lwm2m_os_storage_read(LWM2M_STORAGE_ID_MISC, p_data, sizeof(*p_data));
    if (read != sizeof(lwm2m_storage_misc_data_t))
    {
        return -1;
    }

    return 0;
}

int32_t lwm2m_storage_misc_data_store(lwm2m_storage_misc_data_t *p_data)
{
    lwm2m_os_storage_write(LWM2M_STORAGE_ID_MISC, p_data, sizeof(*p_data));
    return 0;
}

static int lwm2m_storage_version_load(struct lwm2m_storage_version *ver)
{
    int err;

    err = lwm2m_os_storage_read(LWM2M_STORAGE_ID_VERSION, ver, sizeof(*ver));
    if (err < 0) {
        LWM2M_TRC("Storage version could be determined");
        return err;
    }

    return 0;
}

static int lwm2m_storage_version_store(void)
{
    int err;
    struct lwm2m_storage_version ver = {
        .version = LWM2M_STORAGE_VERSION,
    };

    err = lwm2m_os_storage_write(LWM2M_STORAGE_ID_VERSION, &ver, sizeof(ver));
    if (err < 0 ) {
        LWM2M_ERR("Could not save storage version to flash, err %d", err);
        return err;
    }

    return 0;
}

extern void lwm2m_factory_reset();

static void lwm2m_storage_version_update(uint8_t from_version)
{
    LWM2M_INF("Updating storage version from %d to %d",
              from_version, LWM2M_STORAGE_VERSION);

    switch (from_version) {
    case 0: {
        /* Delete old security and storage instances in flash,
         * at LWM2M_OS_STORAGE_BASE,        10 entries
         * at LWM2M_OS_STORAGE_BASE + 10,   10 entries
         */
        for (uint32_t i = 0; i < 20; i++) {
            lwm2m_os_storage_delete(LWM2M_OS_STORAGE_BASE + i);
        }

        /* Remove the old LWM2M_STORED_CLASS3_APN */
        lwm2m_os_storage_delete(LWM2M_OS_STORAGE_END - 11);

        /* Need to bootstrap again */
        lwm2m_factory_reset();
    } break;
    }
}

static const char *obj_str_get(uint16_t id)
{
    switch (id) {
    case LWM2M_OBJ_SECURITY:
        return "security";
    case LWM2M_OBJ_SERVER:
        return "server";
    case LWM2M_OBJ_ACCESS_CONTROL:
        return "access control";
    case LWM2M_OBJ_APN_CONNECTION_PROFILE:
        return "apn connection profile";
    case LWM2M_OBJ_PORTFOLIO:
        return "portfolio";
    case LWM2M_OBJ_CONN_EXT:
        return "connectivity extension";
    default:
        return "unknown";
    }
};

static int carrier_encode(uint16_t obj, uint16_t id, uint8_t *buf, size_t *size)
{
    int err;

    switch (obj) {
    case LWM2M_OBJ_SECURITY:
        err = tlv_security_carrier_encode(id, buf, size);
        break;
    case LWM2M_OBJ_SERVER:
        err = tlv_server_carrier_encode(id, buf, size);
        break;
    default:
        err = 0;
        *size = 0;
        break;
    }

    if (err) {
        LWM2M_ERR("Failed to encode carrier res %d (len %d), err %d",
                  id, *size, err);
    }

    return err;
}

static int obj_instance_encode(lwm2m_instance_t *inst, uint8_t *buf,
                               size_t *len)
{
    int err;
    size_t tlv_off;
    uint8_t tlv_buf[192];

    tlv_off = sizeof(tlv_buf);

    /* Encode instance into local buffer. */
    err = lwm2m_tlv_instance_encode(tlv_buf, &tlv_off, inst,
                                    false /* do not check permissions */);

    if (err) {
        __ASSERT(false, "Encoding /%d/%d failed (length %d), err %d",
                 inst->object_id, inst->instance_id, tlv_off, err);
        return err;
    }

    if (operator_is_vzw(true) || operator_is_att(true)) {
        /* Encode carrier-specific data */
        size_t carrier_len;
        carrier_len = sizeof(tlv_buf) - tlv_off;
        carrier_encode(inst->object_id, inst->instance_id,
                       tlv_buf + tlv_off, &carrier_len);

        tlv_off += carrier_len;
    }

    /* Encode an outer TLV and copy it into `buf`,
     * together with the encoded TLV entry.
     */
    lwm2m_tlv_t tlv = {
        .id_type = TLV_TYPE_OBJECT,
        .id = inst->instance_id,
        .length = tlv_off,
        .value = tlv_buf
    };

    err = lwm2m_tlv_encode(buf, len, &tlv);
    if (err) {
        __ASSERT(false, "Encoding %s instances failed, err %d",
                         obj_str_get(inst->object_id), err);
        return err;
    }

    return 0;
}

static bool skip_instance(uint16_t obj, uint16_t inst)
{
    switch (obj) {
    case LWM2M_OBJ_PORTFOLIO:
        if (inst == LWM2M_PORTFOLIO_HOST_DEVICE_INSTANCE) {
            return true;
        }
    default:
        return false;
    }
}

static int obj_instances_encode(uint16_t obj, uint8_t *buf, size_t *len)
{
    int err;
    size_t off;
    size_t progress;
    lwm2m_instance_t *inst;

    off = 0;
    inst = NULL;
    const size_t buf_len = *len;

    /* Encode all instances of given object */
    while (lwm2m_instance_next(&inst, &progress)) {
        if (inst->object_id != obj) {
            continue;
        }

        if (skip_instance(obj, inst->instance_id)) {
            continue;
        }

        *len = buf_len - off; /* bytes left in `buf` */

        err = obj_instance_encode(inst, &buf[off], len);
        if (err) {
            return err;
        }

        off += *len;

        LWM2M_TRC("Encoded /%d/%d in %d bytes, off %d",
                  inst->object_id, inst->instance_id, *len, off);
    }

    *len = off;

    return 0;
}

static void obj_instance_add_to_handler(uint16_t obj, uint16_t inst)
{
    lwm2m_instance_t *p_instance = NULL;

    switch (obj) {
    case LWM2M_OBJ_ACCESS_CONTROL:
        p_instance = (lwm2m_instance_t *)lwm2m_access_control_get_instance(inst);
        break;
    case LWM2M_OBJ_APN_CONNECTION_PROFILE:
        p_instance = (lwm2m_instance_t *)lwm2m_apn_conn_prof_get_instance(inst);
        break;
    case LWM2M_OBJ_PORTFOLIO:
        /* A Portfolio instance created with a custom instance ID. */
        if (inst >= LWM2M_PORTFOLIO_MAX_INSTANCES) {
            p_instance = (lwm2m_instance_t *)lwm2m_portfolio_get_instance(LWM2M_PORTFOLIO_CARRIER_INSTANCE);
            p_instance->instance_id = inst;
        } else {
            p_instance = (lwm2m_instance_t *)lwm2m_portfolio_get_instance(inst);
        }
        break;
    case LWM2M_OBJ_CONN_EXT:
        p_instance = (lwm2m_instance_t *)lwm2m_conn_ext_get_instance(inst);
        break;
    default:
        break;
    }

    if (p_instance) {
        lwm2m_coap_handler_instance_add(p_instance);
    }
}

static int obj_instances_decode(uint16_t obj, uint8_t *buf, size_t *size)
{
    int err;
    size_t off;
    lwm2m_tlv_t tlv;
    lwm2m_instance_t *p_instance;

    off = 0;

    while (off < *size) {

        err = lwm2m_tlv_decode(&tlv, &off, buf, *size);
        if (err) {
            LWM2M_ERR("Failed to decode TLV object %d, err %d", obj, err);
            return err;
        }

        LWM2M_TRC("Decoded /%d/%d (%d bytes)", obj, tlv.id, tlv.length);

        /* Some instances might have not been added to the handler during the
           initialization, such as the ones that can be created at runtime. */
        if (lwm2m_lookup_instance(&p_instance, obj, tlv.id) != 0)
        {
            obj_instance_add_to_handler(obj, tlv.id);
        }

        switch (obj) {
        case LWM2M_OBJ_SECURITY:
            err = lwm2m_tlv_security_decode(
                lwm2m_security_get_instance(tlv.id),
                tlv.value, tlv.length, tlv_security_carrier_decode);
            break;
        case LWM2M_OBJ_SERVER:
            err = lwm2m_tlv_server_decode(
                lwm2m_server_get_instance(tlv.id),
                tlv.value, tlv.length, tlv_server_carrier_decode);
            break;
        case LWM2M_OBJ_ACCESS_CONTROL:
            err = lwm2m_tlv_access_control_decode(
                lwm2m_access_control_get_instance(tlv.id),
                tlv.value, tlv.length, NULL);
            break;
        case LWM2M_OBJ_APN_CONNECTION_PROFILE:
            err = lwm2m_tlv_apn_connection_profile_decode(
                lwm2m_apn_conn_prof_get_instance(tlv.id),
                tlv.value, tlv.length, NULL);
            break;
        case LWM2M_OBJ_PORTFOLIO:
            err = lwm2m_tlv_portfolio_decode(
                lwm2m_portfolio_get_instance(tlv.id),
                tlv.value, tlv.length, NULL);
            break;
        case LWM2M_OBJ_CONN_EXT:
            err = lwm2m_tlv_connectivity_extension_decode(
                lwm2m_conn_ext_get_instance(tlv.id),
                tlv.value, tlv.length, NULL);
            break;
        }

        if (err) {
            LWM2M_ERR("Failed to decode /%d/%d, err %d", obj, tlv.id, err);
            return err;
        }
    }

    return 0;
}

static uint16_t storage_id_get(uint16_t obj)
{
    switch (obj) {
    case LWM2M_OBJ_SECURITY:
        return LWM2M_STORAGE_ID_SECURITY;
    case LWM2M_OBJ_SERVER:
        return LWM2M_STORAGE_ID_SERVER;
    case LWM2M_OBJ_ACCESS_CONTROL:
        return LWM2M_STORAGE_ID_ACL;
    case LWM2M_OBJ_APN_CONNECTION_PROFILE:
        return LWM2M_STORAGE_APN_CONN_PROFILE;
    case LWM2M_OBJ_PORTFOLIO:
        return LWM2M_STORAGE_PORTFOLIO;
    case LWM2M_OBJ_CONN_EXT:
        return LWM2M_STORAGE_CONN_EXTENSION;
    default:
        __ASSERT_NO_MSG(false);
        return 0;
    }
}

static int lwm2m_storage_obj_instances_store(uint16_t obj)
{
    int err;
    size_t len;
    uint16_t storage_id;

    __ASSERT(obj == LWM2M_OBJ_SECURITY ||
             obj == LWM2M_OBJ_SERVER ||
             obj == LWM2M_OBJ_ACCESS_CONTROL ||
             obj == LWM2M_OBJ_APN_CONNECTION_PROFILE ||
             obj == LWM2M_OBJ_PORTFOLIO ||
             obj == LWM2M_OBJ_CONN_EXT,
            "Tried to store unexpected object %d", obj);

    len = sizeof(buf);

    err = obj_instances_encode(obj, buf, &len);
    if (err) {
        return err;
    }

    storage_id = storage_id_get(obj);

    err = lwm2m_os_storage_write(storage_id, buf, len);
    if (err < 0) {
        LWM2M_ERR("Failed to store %s object, err %d", obj_str_get(obj), err);
        return err;
    }

    if (err == 0) {
        LWM2M_WRN("Storing %s instances (len %d), no change", obj_str_get(obj), len);
    }

    return 0;
}

static int lwm2m_storage_obj_instances_load(uint16_t obj)
{
    int read;
    size_t len;
    uint16_t storage_id;

    __ASSERT(obj == LWM2M_OBJ_SECURITY ||
             obj == LWM2M_OBJ_SERVER ||
             obj == LWM2M_OBJ_ACCESS_CONTROL ||
             obj == LWM2M_OBJ_APN_CONNECTION_PROFILE ||
             obj == LWM2M_OBJ_PORTFOLIO ||
             obj == LWM2M_OBJ_CONN_EXT,
            "Tried to load unexpected object %d", obj);

    len = sizeof(buf);
    storage_id = storage_id_get(obj);

    read = lwm2m_os_storage_read(storage_id, buf, len);
    if (read < 1) {
        LWM2M_TRC("Failed to read %s objects, err %d", obj_str_get(obj), read);
        return -1;
    }

    return obj_instances_decode(obj, buf, &read);
}

static int lwm2m_storage_obj_instances_delete(uint16_t obj)
{
    int err;
    uint16_t storage_id;

    storage_id = storage_id_get(obj);
    err = lwm2m_os_storage_delete(storage_id);
    if (err) {
        LWM2M_WRN("Failed to delete %s instances from flash, err %d",
                   obj_str_get(obj), err);
        return err;
    }

    return 0;
}

int lwm2m_storage_security_load(void)
{
    return lwm2m_storage_obj_instances_load(LWM2M_OBJ_SECURITY);
}

int lwm2m_storage_security_store(void)
{
    return lwm2m_storage_obj_instances_store(LWM2M_OBJ_SECURITY);
}

int lwm2m_storage_security_delete(void)
{
    return lwm2m_storage_obj_instances_delete(LWM2M_OBJ_SECURITY);
}

int lwm2m_storage_server_load(void)
{
    return lwm2m_storage_obj_instances_load(LWM2M_OBJ_SERVER);
}

int lwm2m_storage_server_store(void)
{
    return lwm2m_storage_obj_instances_store(LWM2M_OBJ_SERVER);
}

int lwm2m_storage_server_delete(void)
{
    return lwm2m_storage_obj_instances_delete(LWM2M_OBJ_SERVER);
}

int lwm2m_storage_apn_conn_prof_store(void)
{
    return lwm2m_storage_obj_instances_store(LWM2M_OBJ_APN_CONNECTION_PROFILE);
}

int lwm2m_storage_apn_conn_prof_load(void)
{
    return lwm2m_storage_obj_instances_load(LWM2M_OBJ_APN_CONNECTION_PROFILE);
}

int lwm2m_storage_apn_conn_prof_delete(void)
{
    return lwm2m_storage_obj_instances_delete(LWM2M_OBJ_APN_CONNECTION_PROFILE);
}

int lwm2m_storage_portfolio_store(void)
{
    return lwm2m_storage_obj_instances_store(LWM2M_OBJ_PORTFOLIO);
}

int lwm2m_storage_portfolio_load(void)
{
    return lwm2m_storage_obj_instances_load(LWM2M_OBJ_PORTFOLIO);
}

int lwm2m_storage_portfolio_delete(void)
{
    return lwm2m_storage_obj_instances_delete(LWM2M_OBJ_PORTFOLIO);
}

int lwm2m_storage_conn_ext_store(void)
{
    return lwm2m_storage_obj_instances_store(LWM2M_OBJ_CONN_EXT);
}

int lwm2m_storage_conn_ext_load(void)
{
    return lwm2m_storage_obj_instances_load(LWM2M_OBJ_CONN_EXT);
}

int lwm2m_storage_conn_ext_delete(void)
{
    return lwm2m_storage_obj_instances_delete(LWM2M_OBJ_CONN_EXT);
}

int lwm2m_storage_access_control_load(void)
{
    return lwm2m_storage_obj_instances_load(LWM2M_OBJ_ACCESS_CONTROL);
}

int lwm2m_storage_access_control_store(void)
{
    return lwm2m_storage_obj_instances_store(LWM2M_OBJ_ACCESS_CONTROL);
}

int lwm2m_storage_access_control_delete(void)
{
    return lwm2m_storage_obj_instances_delete(LWM2M_OBJ_ACCESS_CONTROL);
}

int lwm2m_storage_location_load(void)
{
    int err;
    void *loc;
    size_t len;

    lwm2m_remote_location_get(&loc, &len);

    err = lwm2m_os_storage_read(LWM2M_STORAGE_ID_LOCATION, loc, len);
    if (err < 0) {
        LWM2M_TRC("Failed to load location data, err %d", err);
        return -1;
    }

    return 0;
}

int lwm2m_storage_location_store(void)
{
    int err;
    void *loc;
    size_t len;

    lwm2m_remote_location_get(&loc, &len);

    err = lwm2m_os_storage_write(LWM2M_STORAGE_ID_LOCATION, loc, len);
    if (err < 0) {
        LWM2M_ERR("Failed to store location data, err %d", err);
        return -1;
    }

    return 0;
}

int lwm2m_storage_location_delete(void)
{
    int err = lwm2m_os_storage_delete(LWM2M_STORAGE_ID_LOCATION);
    if (err < 0) {
        LWM2M_ERR("Failed to delete location data, err %d", err);
        return -1;
    }

    return 0;
}

int32_t lwm2m_last_used_msisdn_get(char * p_msisdn, uint8_t max_len)
{
    return lwm2m_os_storage_read(LWM2M_STORAGE_ID_MSISDN, p_msisdn, max_len);
}

int32_t lwm2m_last_used_msisdn_set(const char * p_msisdn, uint8_t len)
{
    return lwm2m_os_storage_write(LWM2M_STORAGE_ID_MSISDN, p_msisdn, len);
}

int32_t lwm2m_last_used_operator_id_get(uint32_t * p_operator_id)
{
    return lwm2m_os_storage_read(LWM2M_STORAGE_OPERATOR_ID, p_operator_id, sizeof(*p_operator_id));
}

int32_t lwm2m_last_used_operator_id_set(uint32_t operator_id)
{
    return lwm2m_os_storage_write(LWM2M_STORAGE_OPERATOR_ID, &operator_id, sizeof(operator_id));
}

int32_t lwm2m_debug_settings_load(debug_settings_t * debug_settings)
{
    return lwm2m_os_storage_read(LWM2M_STORAGE_ID_DEBUG, debug_settings, sizeof(*debug_settings));
}

int32_t lwm2m_debug_settings_store(const debug_settings_t * debug_settings)
{
    return lwm2m_os_storage_write(LWM2M_STORAGE_ID_DEBUG, debug_settings, sizeof(*debug_settings));
}

int lwm2m_last_firmware_version_get(uint8_t *ver, size_t len)
{
    int rc;

    __ASSERT_NO_MSG(len == sizeof(nrf_dfu_fw_version_t));

    len = sizeof(nrf_dfu_fw_version_t);
    rc = lwm2m_os_storage_read(LWM2M_MODEM_FIRMWARE_VERSION, ver, len);
    if (rc < 0)
    {
        LWM2M_TRC("Unable to read modem firmware version to flash, err %d", rc);
        return rc;
    }

    return 0;
}

int lwm2m_last_firmware_version_set(uint8_t *ver, size_t len)
{
    int rc;

    __ASSERT_NO_MSG(len == sizeof(nrf_dfu_fw_version_t));

    len = sizeof(nrf_dfu_fw_version_t);
    rc = lwm2m_os_storage_write(LWM2M_MODEM_FIRMWARE_VERSION, ver, len);
    if (rc < 0)
    {
        LWM2M_ERR("Unable to write modem firmware version to flash, err %d", rc);
        return rc;
    }

    return 0;
}

int lwm2m_firmware_image_state_get(enum lwm2m_firmware_image_state *state)
{
    int rc;

    rc = lwm2m_os_storage_read(LWM2M_MODEM_FIRMWARE_READY, state, sizeof(*state));
    if (rc < 0)
    {
        LWM2M_TRC("Unable to find modem firmware state in flash, err %d", rc);
        return rc;
    }

    return 0;
}

int lwm2m_firmware_image_state_set(enum lwm2m_firmware_image_state state)
{
    int rc;

    rc = lwm2m_os_storage_write(LWM2M_MODEM_FIRMWARE_READY, &state, sizeof(state));
    if (rc < 0)
    {
        LWM2M_ERR("Unable to write modem firmware state to flash, err %d", rc);
        return rc;
    }

    return 0;
}

int lwm2m_firmware_update_state_get(enum lwm2m_firmware_update_state *state)
{
    int rc;

    rc = lwm2m_os_storage_read(LWM2M_MODEM_FIRMWARE_UPDATE, state, sizeof(*state));
    if (rc < 0)
    {
        LWM2M_TRC("lwm2m_firmware_update_ready_get() not found, err %d", rc);
        return rc;
    }

    return 0;
}

int lwm2m_firmware_update_state_set(enum lwm2m_firmware_update_state state)
{
    int rc;

    rc = lwm2m_os_storage_write(LWM2M_MODEM_FIRMWARE_UPDATE, &state, sizeof(state));
    if (rc < 0)
    {
        LWM2M_ERR("Unable to write modem firmware info to flash, err %d", rc);
        return rc;
    }

    return 0;
}

int lwm2m_firmware_uri_get(char *uri, size_t *len)
{
    int rc;

    rc = lwm2m_os_storage_read(LWM2M_MODEM_FIRMWARE_URI, uri, *len);
    if (rc < 0)
    {
        LWM2M_TRC("lwm2m_firmware_uri_get(), err %d", rc);
        return rc;
    }

    *len = rc; /* bytes read */

    return 0;
}

int lwm2m_firmware_uri_set(char *uri, size_t len)
{
    int rc;

    rc = lwm2m_os_storage_write(LWM2M_MODEM_FIRMWARE_URI, uri, len);
    if (rc < 0)
    {
        LWM2M_ERR("lwm2m_firmware_uri_set(), err %d", rc);
        return rc;
    }

    return 0;
}

int lwm2m_stored_class3_apn_read(char *class3_apn, size_t len)
{
    return lwm2m_os_storage_read(LWM2M_STORED_CLASS3_APN, class3_apn, len);
}

int lwm2m_stored_class3_apn_write(char *class3_apn, size_t len)
{
    return lwm2m_os_storage_write(LWM2M_STORED_CLASS3_APN, class3_apn, len);
}

int lwm2m_stored_class3_apn_delete(void)
{
    return lwm2m_os_storage_delete(LWM2M_STORED_CLASS3_APN);
}

int lwm2m_observer_store(uint32_t sid, void * data, size_t size)
{
    int rc;

    rc = lwm2m_os_storage_write(LWM2M_OBSERVERS_BASE + sid, data, size);
    if (rc < 0)
    {
        return rc;
    }

    return 0;
}

int lwm2m_observer_load(uint32_t sid, void * data, size_t size)
{
    int rc;

    rc = lwm2m_os_storage_read(LWM2M_OBSERVERS_BASE + sid, data, size);
    if (rc < 0)
    {
        return rc;
    }

    return 0;
}

int lwm2m_observer_delete(uint32_t sid)
{
    int rc = lwm2m_os_storage_delete(LWM2M_OBSERVERS_BASE + sid);
    if (rc < 0)
    {
        return rc;
    }

    return 0;
}

int lwm2m_notif_attr_store(uint32_t sid, void * data, size_t size)
{
    int rc;

    rc = lwm2m_os_storage_write(LWM2M_NOTIF_ATTR_BASE + sid, data, size);
    if (rc < 0)
    {
        return rc;
    }

    return 0;
}

int lwm2m_notif_attr_load(uint32_t sid, void * data, size_t size)
{
    int rc;

    rc = lwm2m_os_storage_read(LWM2M_NOTIF_ATTR_BASE + sid, data, size);
    if (rc < 0)
    {
        return rc;
    }

    return 0;
}

int lwm2m_notif_attr_delete(uint32_t sid)
{
    int rc = lwm2m_os_storage_delete(LWM2M_NOTIF_ATTR_BASE + sid);
    if (rc < 0)
    {
        return rc;
    }

    return 0;
}
