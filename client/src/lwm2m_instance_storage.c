#include <stdint.h>
#include <stddef.h>
#include <zephyr.h>
#include <nvs/nvs.h>
#include <lwm2m.h>
#include <lwm2m_api.h>
#include <lwm2m_objects.h>

#include <lwm2m_instance_storage.h>

#include <lwm2m_security.h>
#include <lwm2m_server.h>

#if CONFIG_FLASH

/* NVS-related defines */
#define NVS_SECTOR_SIZE    FLASH_ERASE_BLOCK_SIZE    /* Multiple of FLASH_PAGE_SIZE */
#define NVS_SECTOR_COUNT   2                         /* At least 2 sectors */
#define NVS_STORAGE_OFFSET FLASH_AREA_STORAGE_OFFSET /* Start address of the filesystem in flash */

static struct nvs_fs fs = {
    .sector_size  = NVS_SECTOR_SIZE,
    .sector_count = NVS_SECTOR_COUNT,
    .offset       = NVS_STORAGE_OFFSET,
};

#define LWM2M_INSTANCE_STORAGE_TYPE_MAX_COUNT 10
#define LWM2M_INSTANCE_STORAGE_MISC_DATA       1
#define LWM2M_INSTANCE_STORAGE_MSISDN          2
#define LWM2M_INSTANCE_STORAGE_BASE_SECURITY  (1 * LWM2M_INSTANCE_STORAGE_TYPE_MAX_COUNT)
#define LWM2M_INSTANCE_STORAGE_BASE_SERVER    (2 * LWM2M_INSTANCE_STORAGE_TYPE_MAX_COUNT)

typedef struct __attribute__((__packed__))
{
    uint8_t bootstrapped;
} storage_misc_data_t;

typedef struct __attribute__((__packed__))
{
    uint8_t  uri_len;
    char     uri[];
} storage_security_t;

typedef struct __attribute__((__packed__))
{
    uint16_t short_server_id;
    uint32_t lifetime;
} storage_server_t;

int32_t lwm2m_instance_storage_init(void)
{
    int rc = nvs_init(&fs, DT_FLASH_DEV_NAME);
    if (rc)
    {
        return -1;
    }
    return 0;
}

int32_t lwm2m_instance_storage_deinit(void)
{
    return 0;
}

int32_t lwm2m_instance_storage_misc_data_load(lwm2m_instance_storage_misc_data_t * p_value)
{
    ssize_t read_count = nvs_read(&fs, LWM2M_INSTANCE_STORAGE_MISC_DATA, p_value, sizeof(lwm2m_instance_storage_misc_data_t));
    if (read_count != sizeof(lwm2m_instance_storage_misc_data_t))
    {
        return -1;
    }
    return 0;
}

int32_t lwm2m_instance_storage_misc_data_store(lwm2m_instance_storage_misc_data_t * p_value)
{
    nvs_write(&fs, LWM2M_INSTANCE_STORAGE_MISC_DATA, p_value, sizeof(lwm2m_instance_storage_misc_data_t));
    return 0;
}

int32_t lwm2m_instance_storage_misc_data_delete(void)
{
    nvs_delete(&fs, LWM2M_INSTANCE_STORAGE_MISC_DATA);
    return 0;
}

int32_t lwm2m_instance_storage_security_load(uint16_t instance_id)
{
    storage_security_t head;
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SECURITY + instance_id;

    uint32_t head_len = offsetof(storage_security_t, uri);
    ssize_t read_count = nvs_read(&fs, id, &head, head_len);

    if (read_count != head_len)
    {
        return -1;
    }

    uint16_t full_len = head_len + head.uri_len;
    storage_security_t * p_storage_security = (storage_security_t *)lwm2m_malloc(full_len);
    read_count = nvs_read(&fs, id, p_storage_security, full_len);

    uint32_t err_code = 0;
    if (read_count != full_len)
    {
        err_code = -1;
    }

    if (err_code == 0)
    {
        if (p_storage_security->uri_len)
        {
            lwm2m_security_server_uri_set(instance_id, p_storage_security->uri, p_storage_security->uri_len);
        }
    }

    lwm2m_free(p_storage_security);

    return err_code;
}

int32_t lwm2m_instance_storage_security_store(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SECURITY + instance_id;

    uint8_t uri_len;
    char * uri = lwm2m_security_server_uri_get(instance_id, &uri_len);

    uint32_t head_len = offsetof(storage_security_t, uri);
    uint8_t full_len = head_len + uri_len;
    storage_security_t * p_storage_security = (storage_security_t *)lwm2m_malloc(full_len);

    memcpy(&p_storage_security->uri[0], uri, uri_len);
    p_storage_security->uri_len = uri_len;

    nvs_write(&fs, id, p_storage_security, full_len);

    lwm2m_free(p_storage_security);

    return 0;
}

int32_t lwm2m_instance_storage_security_delete(uint16_t instance_id)
{
    u16_t id = LWM2M_INSTANCE_STORAGE_BASE_SECURITY + instance_id;
    nvs_delete(&fs, id);
    return 0;
}

int32_t lwm2m_instance_storage_server_load(uint16_t instance_id)
{
    return 0;
}

int32_t lwm2m_instance_storage_server_store(uint16_t instance_id)
{
    return 0;
}

int32_t lwm2m_instance_storage_server_delete(uint16_t instance_id)
{
    return 0;
}


int32_t lwm2m_last_used_msisdn_get(char * p_msisdn, uint8_t max_len)
{
    return nvs_read(&fs, LWM2M_INSTANCE_STORAGE_MSISDN, p_msisdn, max_len);
}

int32_t lwm2m_last_used_msisdn_set(const char * p_msisdn, uint8_t len)
{
    return nvs_write(&fs, LWM2M_INSTANCE_STORAGE_MSISDN, p_msisdn, len);
}

#endif // CONFIG_FLASH