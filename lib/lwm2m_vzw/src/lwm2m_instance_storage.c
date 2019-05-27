#include <stdint.h>
#include <stddef.h>

#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>
#include <lwm2m_acl.h>

#include <lwm2m_instance_storage.h>

#include <lwm2m_security.h>
#include <lwm2m_server.h>
#include <lwm2m_device.h>
#include <lwm2m_conn_mon.h>
#include <lwm2m_firmware.h>

#include <lwm2m_os.h>

#define LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET   0xFFFF

#define LWM2M_INSTANCE_STORAGE_TYPE_MAX_COUNT 10
#define LWM2M_INSTANCE_STORAGE_MISC_DATA       1
#define LWM2M_INSTANCE_STORAGE_DEVICE          2
#define LWM2M_INSTANCE_STORAGE_CONN_MON        3
#define LWM2M_INSTANCE_STORAGE_FIRMWARE        4
#define LWM2M_INSTANCE_STORAGE_MSISDN          8
#define LWM2M_INSTANCE_STORAGE_DEBUG_SETTINGS  9
#define LWM2M_INSTANCE_STORAGE_BASE_SECURITY  (1 * LWM2M_INSTANCE_STORAGE_TYPE_MAX_COUNT)
#define LWM2M_INSTANCE_STORAGE_BASE_SERVER    (2 * LWM2M_INSTANCE_STORAGE_TYPE_MAX_COUNT)


typedef struct __attribute__((__packed__))
{
    uint8_t bootstrapped;
/*
    // One bit for each server.
    uint8_t server_is_registered_mask;                // Server object extended data?
    uint8_t dfu_state;
    uint8_t world_clock;
    // Identify mode and carrier switch.
    char * msisdn;
    uint8_t carrier_id;
    uint8_t server_retry_timeouts[LWM2M_MAX_SERVERS]; // Server object extended data?
*/
} storage_misc_data_t;

typedef struct __attribute__((__packed__))
{
    uint8_t  boostrap_server;
    int32_t client_hold_off_time;
    uint16_t short_server_id;

    // Offsets into data post static sized values.
    uint16_t offset_uri;
    uint16_t offset_sms_number;
    uint16_t offset_carrier_specific;
} storage_security_t;

typedef struct __attribute__((__packed__))
{
    uint16_t short_server_id;
    int32_t lifetime;
    int32_t default_min_period;
    int32_t default_max_period;
    int32_t disable_timeout;
    uint8_t notif_storing;

    // Offsets into data post static sized values.
    uint16_t offset_binding;
    uint16_t offset_carrier_specific;
    uint16_t offset_acl;
} storage_server_t;

typedef struct __attribute__((__packed__))
{
    // Offsets into data post static sized values.
    uint16_t offset_carrier_specific;
    uint16_t offset_acl;
} storage_device_t;

typedef struct __attribute__((__packed__))
{
    // Offsets into data post static sized values.
    uint16_t offset_carrier_specific;
    uint16_t offset_acl;
} storage_conn_mon_t;

typedef struct __attribute__((__packed__))
{
    // Offsets into data post static sized values.
    uint16_t offset_carrier_specific;
    uint16_t offset_acl;
} storage_firmware_t;

int32_t lwm2m_instance_storage_init(void)
{
    return lwm2m_os_storage_init();
}

int32_t lwm2m_instance_storage_deinit(void)
{
    return 0;
}

int32_t lwm2m_instance_storage_all_objects_load(void)
{
    // Reset ACL module as its shared between all object instances.
    lwm2m_acl_init();

    // Load all objects.
    for (int i = 0; i < (1 + LWM2M_MAX_SERVERS); i++)
    {
        lwm2m_instance_storage_security_load(i);
        lwm2m_instance_storage_server_load(i);
    }
    lwm2m_instance_storage_device_load(0);
    lwm2m_instance_storage_conn_mon_load(0);
    lwm2m_instance_storage_firmware_load(0);

    return 0;
}

int32_t lwm2m_instance_storage_all_objects_store(void)
{
    // Store all objects.
    for (int i = 0; i < (1 + LWM2M_MAX_SERVERS); i++)
    {
        lwm2m_instance_storage_security_store(i);
        lwm2m_instance_storage_server_store(i);
    }
    lwm2m_instance_storage_device_store(0);
    lwm2m_instance_storage_conn_mon_store(0);
    lwm2m_instance_storage_firmware_store(0);

    return 0;
}

int32_t lwm2m_instance_storage_all_objects_delete(void)
{
    // Delete all objects.
    for (int i = 0; i < (1 + LWM2M_MAX_SERVERS); i++)
    {
        lwm2m_instance_storage_security_delete(i);
        lwm2m_instance_storage_server_delete(i);
    }
    lwm2m_instance_storage_device_delete(0);
    lwm2m_instance_storage_conn_mon_delete(0);
    lwm2m_instance_storage_firmware_delete(0);

    return 0;
}

int32_t lwm2m_instance_storage_misc_data_load(lwm2m_instance_storage_misc_data_t * p_value)
{
    ssize_t read_count = lwm2m_os_storage_read(LWM2M_INSTANCE_STORAGE_MISC_DATA, p_value, sizeof(lwm2m_instance_storage_misc_data_t));
    if (read_count != sizeof(lwm2m_instance_storage_misc_data_t))
    {
        return -1;
    }
    return 0;
}

int32_t lwm2m_instance_storage_misc_data_store(lwm2m_instance_storage_misc_data_t * p_value)
{
    lwm2m_os_storage_write(LWM2M_INSTANCE_STORAGE_MISC_DATA, p_value, sizeof(lwm2m_instance_storage_misc_data_t));
    return 0;
}

int32_t lwm2m_instance_storage_misc_data_delete(void)
{
    lwm2m_os_storage_delete(LWM2M_INSTANCE_STORAGE_MISC_DATA);
    return 0;
}

int32_t lwm2m_instance_storage_security_load(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SECURITY + instance_id;

    // Peek file size.
    char peak_buffer[1];
    ssize_t read_count = lwm2m_os_storage_read(id, peak_buffer, 1);

    if (read_count <= 0) {
        return -read_count;
    }

    // Read full entry.
    uint8_t * p_scratch_buffer = lwm2m_malloc(read_count);
    read_count = lwm2m_os_storage_read(id, p_scratch_buffer, read_count);
    (void)read_count;

    storage_security_t * p_storage_security = (storage_security_t *)p_scratch_buffer;

    // Set static sized values.
    lwm2m_security_is_bootstrap_server_set(instance_id, p_storage_security->boostrap_server);
    lwm2m_security_client_hold_off_time_set(instance_id, p_storage_security->client_hold_off_time);
    lwm2m_security_short_server_id_set(instance_id, p_storage_security->short_server_id);

    // Set URI.
    uint8_t uri_len = p_storage_security->offset_sms_number - p_storage_security->offset_uri;
    lwm2m_security_server_uri_set(instance_id, &p_scratch_buffer[p_storage_security->offset_uri], uri_len);

    // Set SMS number.
    uint8_t sms_number_len = p_storage_security->offset_carrier_specific - p_storage_security->offset_sms_number;
    lwm2m_security_sms_number_set(instance_id, &p_scratch_buffer[p_storage_security->offset_sms_number], sms_number_len);

    // Set carrier specific data if bootstrap server.
    if (p_storage_security->offset_carrier_specific != LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET)
    {
        vzw_bootstrap_security_settings_t * p_data_carrier_specific = NULL;
        p_data_carrier_specific = (vzw_bootstrap_security_settings_t *)&p_scratch_buffer[p_storage_security->offset_carrier_specific];
        lwm2m_security_bootstrapped_set(instance_id, p_data_carrier_specific->is_bootstrapped);
        lwm2m_security_hold_off_timer_set(instance_id, p_data_carrier_specific->hold_off_timer);
    }

    lwm2m_free(p_scratch_buffer);

    return 0;
}

int32_t lwm2m_instance_storage_security_store(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SECURITY + instance_id;

    // Fetch URI.
    uint8_t uri_len;
    char * uri = lwm2m_security_server_uri_get(instance_id, &uri_len);

    // Fetch SMS number.
    uint8_t sms_number_len;
    char * sms_number = lwm2m_security_sms_number_get(instance_id, &sms_number_len);

    uint16_t total_entry_len = 0;
    total_entry_len += sizeof(storage_security_t);
    total_entry_len += uri_len;
    total_entry_len += sms_number_len;
    total_entry_len += sizeof(lwm2m_instance_acl_t); // Full ACL to be dumped. Due to default ACL index 0.

    storage_security_t temp_storage;

    // Fetch static sized values.
    temp_storage.boostrap_server         = lwm2m_security_is_bootstrap_server_get(instance_id);
    temp_storage.client_hold_off_time    = lwm2m_security_client_hold_off_time_get(instance_id);
    temp_storage.short_server_id         = lwm2m_security_short_server_id_get(instance_id);
    temp_storage.offset_uri              = sizeof(storage_security_t); // Where storage_security_t ends.
    temp_storage.offset_sms_number       = temp_storage.offset_uri + uri_len;

    // If bootstrap server.
    if (instance_id == 0)
    {
        total_entry_len += sizeof(vzw_bootstrap_security_settings_t);
        temp_storage.offset_carrier_specific = temp_storage.offset_sms_number + sms_number_len;
    } else {
        temp_storage.offset_carrier_specific = LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET;
    }

    uint8_t * p_scratch_buffer = lwm2m_malloc(total_entry_len);
    memcpy(p_scratch_buffer, &temp_storage, sizeof(storage_security_t));
    memcpy(&p_scratch_buffer[temp_storage.offset_uri], uri, uri_len);
    memcpy(&p_scratch_buffer[temp_storage.offset_sms_number], sms_number, sms_number_len);

    // If bootstrap server.
    if (temp_storage.offset_carrier_specific != LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET)
    {
        // Fetch carrier specific data.
        vzw_bootstrap_security_settings_t data_carrier_specific;
        data_carrier_specific.is_bootstrapped = lwm2m_security_bootstrapped_get(instance_id);
        data_carrier_specific.hold_off_timer = lwm2m_security_hold_off_timer_get(instance_id);
        memcpy(&p_scratch_buffer[temp_storage.offset_carrier_specific], &data_carrier_specific, sizeof(vzw_bootstrap_security_settings_t));
    }

    lwm2m_os_storage_write(id, p_scratch_buffer, total_entry_len);

    lwm2m_free(p_scratch_buffer);

    return 0;
}

int32_t lwm2m_instance_storage_security_delete(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SECURITY + instance_id;
    lwm2m_os_storage_delete(id);
    return 0;
}

int32_t lwm2m_instance_storage_server_load(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SERVER + instance_id;

    // Peek file size.
    char peak_buffer[1];
    ssize_t read_count = lwm2m_os_storage_read(id, peak_buffer, 1);

    if (read_count <= 0) {
        return -read_count;
    }

    // Read full entry.
    uint8_t * p_scratch_buffer = lwm2m_malloc(read_count);
    read_count = lwm2m_os_storage_read(id, p_scratch_buffer, read_count);
    (void)read_count;

    storage_server_t * p_storage_server = (storage_server_t *)p_scratch_buffer;

    // Set static sized values.
    lwm2m_server_short_server_id_set(instance_id, p_storage_server->short_server_id);
    lwm2m_server_lifetime_set(instance_id, p_storage_server->lifetime);
    lwm2m_server_min_period_set(instance_id, p_storage_server->default_min_period);
    lwm2m_server_max_period_set(instance_id, p_storage_server->default_max_period);
    lwm2m_server_disable_timeout_set(instance_id, p_storage_server->disable_timeout);
    lwm2m_server_notif_storing_set(instance_id, p_storage_server->notif_storing);

    // Set binding.
    uint8_t binding_len = p_storage_server->offset_carrier_specific - p_storage_server->offset_binding;
    lwm2m_server_binding_set(instance_id, &p_scratch_buffer[p_storage_server->offset_binding], binding_len);

    // Set carrier specific data if bootstrap server.
    if (p_storage_server->offset_carrier_specific != LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET)
    {
        vzw_server_settings_t * p_data_carrier_specific = NULL;
        p_data_carrier_specific = (vzw_server_settings_t *)&p_scratch_buffer[p_storage_server->offset_carrier_specific];
        lwm2m_server_registered_set(instance_id, p_data_carrier_specific->is_registered);
        lwm2m_server_client_hold_off_timer_set(instance_id, p_data_carrier_specific->client_hold_off_timer);
    }

    // Write the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = (lwm2m_instance_acl_t *)&p_scratch_buffer[p_storage_server->offset_acl];

    uint32_t err_code = lwm2m_acl_permissions_init(p_instance, p_acl->owner);
    for (uint8_t i = 0; i < (1 + LWM2M_MAX_SERVERS); i++)
    {
        err_code = lwm2m_acl_permissions_add(p_instance, p_acl->access[i], p_acl->server[i]);
    }

    // Override instance id. Experimental.
    lwm2m_instance_acl_t * p_real_acl = &p_instance->acl;
    p_real_acl->id = p_acl->id;

    lwm2m_free(p_scratch_buffer);

    return err_code;
}

int32_t lwm2m_instance_storage_server_store(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SERVER + instance_id;

    // Fetch binding.
    uint8_t binding_len;
    char * binding = lwm2m_server_binding_get(instance_id, &binding_len);

    // Locate the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_server_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = &p_instance->acl;

    uint16_t total_entry_len = 0;
    total_entry_len += sizeof(storage_server_t);
    total_entry_len += binding_len;
    total_entry_len += sizeof(lwm2m_instance_acl_t); // Full ACL to be dumped. Due to default ACL index 0.

    storage_server_t temp_storage;

    // Fetch static sized values.
    temp_storage.short_server_id    = lwm2m_server_short_server_id_get(instance_id);
    temp_storage.lifetime           = lwm2m_server_lifetime_get(instance_id);
    temp_storage.default_min_period = lwm2m_server_min_period_get(instance_id);
    temp_storage.default_max_period = lwm2m_server_max_period_get(instance_id);
    temp_storage.disable_timeout    = lwm2m_server_disable_timeout_get(instance_id);
    temp_storage.notif_storing      = lwm2m_server_notif_storing_get(instance_id);
    temp_storage.offset_binding     = sizeof(storage_server_t); // Where storage_server_t ends.

    // if bootstrap server.
    if (instance_id == 0)
    {
        temp_storage.offset_carrier_specific = LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET;
        temp_storage.offset_acl              = temp_storage.offset_binding + binding_len;
    } else {
        total_entry_len += sizeof(vzw_server_settings_t);
        temp_storage.offset_carrier_specific = temp_storage.offset_binding + binding_len;
        temp_storage.offset_acl              = temp_storage.offset_carrier_specific + sizeof(vzw_server_settings_t);
    }

    uint8_t * p_scratch_buffer = lwm2m_malloc(total_entry_len);
    memcpy(p_scratch_buffer, &temp_storage, sizeof(storage_server_t));
    memcpy(&p_scratch_buffer[temp_storage.offset_binding], binding, binding_len);
    memcpy(&p_scratch_buffer[temp_storage.offset_acl], p_acl, sizeof(lwm2m_instance_acl_t));

    // If not bootstrap server.
    if (temp_storage.offset_carrier_specific != LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET)
    {
        // Fetch carrier specific data.
        vzw_server_settings_t data_carrier_specific;
        data_carrier_specific.is_registered         = lwm2m_security_bootstrapped_get(instance_id);
        data_carrier_specific.client_hold_off_timer = lwm2m_security_client_hold_off_time_get(instance_id);
        memcpy(&p_scratch_buffer[temp_storage.offset_carrier_specific], &data_carrier_specific, sizeof(vzw_server_settings_t));
    }

    lwm2m_os_storage_write(id, p_scratch_buffer, total_entry_len);

    lwm2m_free(p_scratch_buffer);

    return 0;
}

int32_t lwm2m_instance_storage_server_delete(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SERVER + instance_id;
    lwm2m_os_storage_delete(id);
    return 0;
}

int32_t lwm2m_instance_storage_device_load(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_DEVICE + instance_id;

    // Peek file size.
    char peak_buffer[1];
    ssize_t read_count = lwm2m_os_storage_read(id, peak_buffer, 1);

    if (read_count <= 0) {
        return -read_count;
    }

    // Read full entry.
    uint8_t * p_scratch_buffer = lwm2m_malloc(read_count);
    read_count = lwm2m_os_storage_read(id, p_scratch_buffer, read_count);
    (void)read_count;

    storage_device_t * p_storage_device = (storage_device_t *)p_scratch_buffer;

    // Set carrier specific data if bootstrap server.
    if (p_storage_device->offset_carrier_specific != LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET)
    {
        // If there is any carrier specific data. Handle it here.
    }

    // Write the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_device_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = (lwm2m_instance_acl_t *)&p_scratch_buffer[p_storage_device->offset_acl];

    uint32_t err_code = lwm2m_acl_permissions_init(p_instance, p_acl->owner);
    for (uint8_t i = 0; i < (1 + LWM2M_MAX_SERVERS); i++)
    {
        err_code = lwm2m_acl_permissions_add(p_instance, p_acl->access[i], p_acl->server[i]);
    }

    // Override instance id. Experimental.
    lwm2m_instance_acl_t * p_real_acl = &p_instance->acl;
    p_real_acl->id = p_acl->id;

    lwm2m_free(p_scratch_buffer);

    return err_code;
}

int32_t lwm2m_instance_storage_device_store(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_DEVICE + instance_id;

    // Locate the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_device_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = &p_instance->acl;

    uint16_t total_entry_len = 0;
    total_entry_len += sizeof(storage_device_t);
    // TODO: If carrier specific data, add len of it here.
    total_entry_len += sizeof(lwm2m_instance_acl_t); // Full ACL to be dumped. Due to default ACL index 0.

    storage_device_t temp_storage;

    temp_storage.offset_carrier_specific = LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET;
    temp_storage.offset_acl              = sizeof(storage_device_t);

    uint8_t * p_scratch_buffer = lwm2m_malloc(total_entry_len);
    memcpy(p_scratch_buffer, &temp_storage, sizeof(storage_device_t));
    memcpy(&p_scratch_buffer[temp_storage.offset_acl], p_acl, sizeof(lwm2m_instance_acl_t));

    lwm2m_os_storage_write(id, p_scratch_buffer, total_entry_len);

    lwm2m_free(p_scratch_buffer);

    return 0;
}

int32_t lwm2m_instance_storage_device_delete(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_DEVICE + instance_id;
    lwm2m_os_storage_delete(id);
    return 0;
}

int32_t lwm2m_instance_storage_conn_mon_load(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_CONN_MON + instance_id;

    // Peek file size.
    char peak_buffer[1];
    ssize_t read_count = lwm2m_os_storage_read(id, peak_buffer, 1);

    if (read_count <= 0) {
        return -read_count;
    }

    // Read full entry.
    uint8_t * p_scratch_buffer = lwm2m_malloc(read_count);
    read_count = lwm2m_os_storage_read(id, p_scratch_buffer, read_count);
    (void)read_count;

    storage_conn_mon_t * p_storage_conn_mon = (storage_conn_mon_t *)p_scratch_buffer;

    // Set carrier specific data if bootstrap server.
    if (p_storage_conn_mon->offset_carrier_specific != LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET)
    {
        // If there is any carrier specific data. Handle it here.
    }

    // Write the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_conn_mon_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = (lwm2m_instance_acl_t *)&p_scratch_buffer[p_storage_conn_mon->offset_acl];

    uint32_t err_code = lwm2m_acl_permissions_init(p_instance, p_acl->owner);
    for (uint8_t i = 0; i < (1 + LWM2M_MAX_SERVERS); i++)
    {
        err_code = lwm2m_acl_permissions_add(p_instance, p_acl->access[i], p_acl->server[i]);
    }

    // Override instance id. Experimental.
    lwm2m_instance_acl_t * p_real_acl = &p_instance->acl;
    p_real_acl->id = p_acl->id;

    lwm2m_free(p_scratch_buffer);

    return err_code;
}

int32_t lwm2m_instance_storage_conn_mon_store(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_CONN_MON + instance_id;

    // Locate the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_conn_mon_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = &p_instance->acl;

    uint16_t total_entry_len = 0;
    total_entry_len += sizeof(storage_conn_mon_t);
    // TODO: If carrier specific data, add len of it here.
    total_entry_len += sizeof(lwm2m_instance_acl_t); // Full ACL to be dumped. Due to default ACL index 0.

    storage_conn_mon_t temp_storage;

    temp_storage.offset_carrier_specific = LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET;
    temp_storage.offset_acl              = sizeof(storage_conn_mon_t);

    uint8_t * p_scratch_buffer = lwm2m_malloc(total_entry_len);
    memcpy(p_scratch_buffer, &temp_storage, sizeof(storage_conn_mon_t));
    memcpy(&p_scratch_buffer[temp_storage.offset_acl], p_acl, sizeof(lwm2m_instance_acl_t));

    lwm2m_os_storage_write(id, p_scratch_buffer, total_entry_len);

    lwm2m_free(p_scratch_buffer);

    return 0;
}

int32_t lwm2m_instance_storage_conn_mon_delete(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_CONN_MON + instance_id;
    lwm2m_os_storage_delete(id);
    return 0;
}

int32_t lwm2m_instance_storage_firmware_load(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_FIRMWARE + instance_id;

    // Peek file size.
    char peak_buffer[1];
    ssize_t read_count = lwm2m_os_storage_read(id, peak_buffer, 1);

    if (read_count <= 0) {
        return -read_count;
    }

    // Read full entry.
    uint8_t * p_scratch_buffer = lwm2m_malloc(read_count);
    read_count = lwm2m_os_storage_read(id, p_scratch_buffer, read_count);
    (void)read_count;

    storage_firmware_t * p_storage_firmware = (storage_firmware_t *)p_scratch_buffer;

    // Set carrier specific data if bootstrap server.
    if (p_storage_firmware->offset_carrier_specific != LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET)
    {
        // If there is any carrier specific data. Handle it here.
    }

    // Write the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_firmware_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = (lwm2m_instance_acl_t *)&p_scratch_buffer[p_storage_firmware->offset_acl];

    uint32_t err_code = lwm2m_acl_permissions_init(p_instance, p_acl->owner);
    for (uint8_t i = 0; i < (1 + LWM2M_MAX_SERVERS); i++)
    {
        err_code = lwm2m_acl_permissions_add(p_instance, p_acl->access[i], p_acl->server[i]);
    }

    // Override instance id. Experimental.
    lwm2m_instance_acl_t * p_real_acl = &p_instance->acl;
    p_real_acl->id = p_acl->id;

    lwm2m_free(p_scratch_buffer);

    return err_code;
}

int32_t lwm2m_instance_storage_firmware_store(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_FIRMWARE + instance_id;

    // Locate the ACL of the instance.
    lwm2m_instance_t * p_instance = (lwm2m_instance_t *)lwm2m_firmware_get_instance(instance_id);
    lwm2m_instance_acl_t * p_acl = &p_instance->acl;

    uint16_t total_entry_len = 0;
    total_entry_len += sizeof(storage_firmware_t);
    // TODO: If carrier specific data, add len of it here.
    total_entry_len += sizeof(lwm2m_instance_acl_t); // Full ACL to be dumped. Due to default ACL index 0.

    storage_firmware_t temp_storage;

    temp_storage.offset_carrier_specific = LWM2M_INSTANCE_STORAGE_FIELD_NOT_SET;
    temp_storage.offset_acl              = sizeof(storage_firmware_t);

    uint8_t * p_scratch_buffer = lwm2m_malloc(total_entry_len);
    memcpy(p_scratch_buffer, &temp_storage, sizeof(storage_firmware_t));
    memcpy(&p_scratch_buffer[temp_storage.offset_acl], p_acl, sizeof(lwm2m_instance_acl_t));

    lwm2m_os_storage_write(id, p_scratch_buffer, total_entry_len);

    lwm2m_free(p_scratch_buffer);

    return 0;
}

int32_t lwm2m_instance_storage_firmware_delete(uint16_t instance_id)
{
    // Only one instance for now.
    u16_t id = LWM2M_INSTANCE_STORAGE_FIRMWARE;
    lwm2m_os_storage_delete(id);
    return 0;
}

int32_t lwm2m_last_used_msisdn_get(char * p_msisdn, uint8_t max_len)
{
    return lwm2m_os_storage_read(LWM2M_INSTANCE_STORAGE_MSISDN, p_msisdn, max_len);
}

int32_t lwm2m_last_used_msisdn_set(const char * p_msisdn, uint8_t len)
{
    return lwm2m_os_storage_write(LWM2M_INSTANCE_STORAGE_MSISDN, p_msisdn, len);
}

int32_t lwm2m_debug_settings_load(debug_settings_t * debug_settings)
{
    return lwm2m_os_storage_read(LWM2M_INSTANCE_STORAGE_DEBUG_SETTINGS, debug_settings, sizeof(*debug_settings));
}

int32_t lwm2m_debug_settings_store(const debug_settings_t * debug_settings)
{
    return lwm2m_os_storage_write(LWM2M_INSTANCE_STORAGE_DEBUG_SETTINGS, debug_settings, sizeof(*debug_settings));
}
