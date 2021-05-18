#define CONFIG_LWM2M_CARRIER

#include <string.h>
#include <stdlib.h>
#include "unity.h"
#include "stddef.h"
#include "lwm2m_objects.h"

#include "lwm2m_objects.c"

void setUp()
{
}

void tearDown()
{
}

void test_LWM2M_INSTANCE_OFFSET_SET_security(void)
{
    uint8_t offsetof_security_operations = offsetof(lwm2m_security_t, operations);
    uint8_t offsetof_security_operations_ids = offsetof(lwm2m_security_t, resource_ids);

    lwm2m_security_t sec;
    lwm2m_security_t * p_instance = &sec;
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_security_t);

    TEST_ASSERT_EQUAL_UINT8(offsetof_security_operations, sec.proto.operations_offset);
    TEST_ASSERT_EQUAL_UINT8(offsetof_security_operations_ids, sec.proto.resource_ids_offset);
}


void test_LWM2M_INSTANCE_OFFSET_SET_server(void)
{
    uint8_t offsetof_server_operations = offsetof(lwm2m_server_t, operations);
    uint8_t offsetof_server_operations_ids = offsetof(lwm2m_server_t, resource_ids);

    lwm2m_server_t ser;
    lwm2m_server_t * p_instance = &ser;
    LWM2M_INSTANCE_OFFSET_SET(p_instance, lwm2m_server_t);

    TEST_ASSERT_EQUAL_UINT8(offsetof_server_operations, ser.proto.operations_offset);
    TEST_ASSERT_EQUAL_UINT8(offsetof_server_operations_ids, ser.proto.resource_ids_offset);
}

extern int unity_main(void);

void main(void)
{
	(void)unity_main();
}
