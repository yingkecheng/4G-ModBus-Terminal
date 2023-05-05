/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-04-29     David       the first version
 */
#include "mqtt_ctl.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define DBG_TAG "mqtt_ctl"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#define MQTT_BUFFER_SIZE 256
#define MQTT_RESP_SIZE 256
#define MQTT_RESP_TIMEOUT 2000

static int mqtt_ctl_cfg(mqtt_ctl_t handler);
static int mqtt_ctl_open(mqtt_ctl_t handler);
static int mqtt_ctl_close(mqtt_ctl_t handler);
static int mqtt_ctl_conn(mqtt_ctl_t handler);
static int mqtt_ctl_disconn(mqtt_ctl_t handler);
static int mqtt_ctl_sub(mqtt_ctl_t handler);
static int mqtt_ctl_unsub(mqtt_ctl_t handler);
static int mqtt_ctl_pubex(mqtt_ctl_t handler, const char *buf, int buf_size);

static void urc_stat_func(struct at_client *client, const char *data, rt_size_t size);
static void urc_open_func(struct at_client *client, const char *data, rt_size_t size);
static void urc_close_func(struct at_client *client, const char *data, rt_size_t size);
static void urc_conn_func(struct at_client *client, const char *data, rt_size_t size);
static void urc_disc_func(struct at_client *client, const char *data, rt_size_t size);
static void urc_sub_func(struct at_client *client, const char *data, rt_size_t size);
static void urc_uns_func(struct at_client *client, const char *data, rt_size_t size);
static void urc_pubex_func(struct at_client *client, const char *data, rt_size_t size);

static int mqtt_urc_init(void);

static struct at_urc urc_table[] =
    {
        {"+QMTSTAT", "\r\n", urc_stat_func},
        {"+QMTOPEN", "\r\n", urc_open_func},
        {"+QMTCLOSE", "\r\n", urc_close_func},
        {"+QMTCONN", "\r\n", urc_conn_func},
        {"+QMTDISC", "\r\n", urc_disc_func},
        {"+QMTSUB", "\r\n", urc_sub_func},
        {"+QMTUNS", "\r\n", urc_uns_func},
        {"+QMTPUBEX", "\r\n", urc_pubex_func},
};

static int mqtt_urc_init(void)
{
    at_set_urc_table(urc_table, sizeof(urc_table) / sizeof(urc_table[0]));
    return 0;
}

static mqtt_ctl_t my_handler = NULL;

mqtt_ctl_t mqtt_ctl_create(void)
{
    mqtt_ctl_t handler = (mqtt_ctl_t)rt_malloc(sizeof(struct mqtt_ctl));

    if (handler == NULL)
    {
        LOG_E("Failed to allocate memory for MQTT control.");
        goto error;
    }

    rt_memset(handler, 0, sizeof(struct mqtt_ctl));

    const char *uart_name = "uart2";
    if (at_client_init(uart_name, MQTT_BUFFER_SIZE) != 0)
    {
        LOG_E("AT client initialization failed.");
        goto error;
    }

    handler->mqtt_resp = at_create_resp(MQTT_RESP_SIZE, 0, MQTT_RESP_TIMEOUT);
    if (handler->mqtt_resp == NULL)
    {
        LOG_E("Failed to create MQTT response object.");
        goto error;
    }

    handler->buf = rt_calloc(MQTT_BUFFER_SIZE, 1);
    handler->buf_size = MQTT_BUFFER_SIZE;
    if (handler->buf == NULL)
    {
        LOG_E("Failed to allocate memory for MQTT control handler buffer.");
        goto error;
    }

    mqtt_urc_init();

    handler->cfg = mqtt_ctl_cfg;
    handler->open = mqtt_ctl_open;
    handler->conn = mqtt_ctl_conn;
    handler->pubex = mqtt_ctl_pubex;

    int res = at_exec_cmd(handler->mqtt_resp, "AT");
    if (res != 0)
    {
        LOG_E("Failed to execute at_exec_cmd.");
        goto error;
    }

    if (!strcmp(at_resp_get_line(handler->mqtt_resp, 1), "OK"))
    {
        LOG_E("AT response error.");
        goto error;
    }

    return handler;

error:
    mqtt_ctl_delete(handler);
    return NULL;
}

void mqtt_ctl_delete(mqtt_ctl_t handler)
{
    if (handler->buf)
    {
        rt_free(handler->buf);
    }

    if (handler->mqtt_resp)
    {
        at_delete_resp(handler->mqtt_resp);
    }

    rt_free(handler);
}

/**
 * mqtt_ctl_init - Initialize the MQTT control handler
 * @handler: mqtt_ctl_t handler instance
 *
 * Return: 0 on success, -1 on failure
 */
int mqtt_ctl_init(mqtt_ctl_t handler)
{
    if (handler == NULL)
    {
        LOG_E("The input pointer is NULL.");
        goto error;
    }

    const char *uart_name = "uart2";
    if (at_client_init(uart_name, MQTT_BUFFER_SIZE) != 0)
    {
        LOG_E("AT client initialization failed.");
        goto error;
    }

    handler->mqtt_resp = at_create_resp(MQTT_RESP_SIZE, 0, MQTT_RESP_TIMEOUT);
    if (handler->mqtt_resp == NULL)
    {
        LOG_E("Failed to create MQTT response object.");
        goto error;
    }

    handler->buf = rt_calloc(MQTT_BUFFER_SIZE, 1);
    handler->buf_size = MQTT_BUFFER_SIZE;
    if (handler->buf == NULL)
    {
        LOG_E("Failed to allocate memory for MQTT control handler buffer.");
        goto error;
    }
    else
    {
        LOG_D("buf address: 0x%x.", (uint32_t)handler->buf);
    }

    handler->cfg = mqtt_ctl_cfg;
    handler->open = mqtt_ctl_open;
    handler->conn = mqtt_ctl_conn;

    int res = at_exec_cmd(handler->mqtt_resp, "AT");
    if (res != 0)
    {
        LOG_E("Failed to execute at_exec_cmd.");
        goto error;
    }

    if (!strcmp(at_resp_get_line(handler->mqtt_resp, 1), "OK"))
    {
        LOG_E("AT response error.");
        goto error;
    }

    return 0;

error:
    mqtt_ctl_deinit(handler);
    return -1;
}

/**
 * mqtt_ctl_deinit - Deinitialize the MQTT control handler and free resources
 * @handler: mqtt_ctl_t handler instance
 */
void mqtt_ctl_deinit(mqtt_ctl_t handler)
{
    if (handler->buf)
    {
        rt_free(handler->buf);
    }
    if (handler->mqtt_resp)
    {
        at_delete_resp(handler->mqtt_resp);
        handler->mqtt_resp = NULL;
    }
}

/**
 * mqtt_ctl_cfg - Configure MQTT settings
 * @handler: mqtt_ctl_t handler instance
 *
 * Return: 0 on success, -1 on failure
 */
static int mqtt_ctl_cfg(mqtt_ctl_t handler)
{
    const char *mqtt_config_cmd = "AT+QMTCFG=\"aliauth\",0,a1mRa3t2xvm,dev_1,92664c8f6a77a8e52d35866dcf4d6737";

    rt_snprintf(handler->buf, handler->buf_size, "%s", mqtt_config_cmd);

    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    if (res == 0)
    {
        const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
        return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
    }

    return -1;
}

static int mqtt_ctl_open(mqtt_ctl_t handler)
{
    const char *mqtt_open_cmd = "AT+QMTOPEN=0,a1mRa3t2xvm.iot-as-mqtt.cn-shanghai.aliyuncs.com,1883";

    rt_snprintf(handler->buf, handler->buf_size, "%s", mqtt_open_cmd);

    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    if (res == 0)
    {
        const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
        return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
    }
    return -1;
}

static int mqtt_ctl_close(mqtt_ctl_t handler)
{
    const char *mqtt_close_cmd = "AT+QMTDISC=0";

    rt_snprintf(handler->buf, handler->buf_size, "%s", mqtt_open_cmd);

    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    if (res == 0)
    {
        const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
        return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
    }
    return -1;
}

static int mqtt_ctl_conn(mqtt_ctl_t handler)
{
    const char *mqtt_conn_cmd = "AT+QMTCONN=0,a1mRa3t2xvm.dev_1,dev_1&a1mRa3t2xvm,2917e1b3dc0311553579238cb78840fbb53f4bfbf188b713558a98f18f271556";

    rt_snprintf(handler->buf, handler->buf_size, "%s", mqtt_conn_cmd);

    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    if (res == 0)
    {
        const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
        return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
    }
    return -1;
}

static int mqtt_ctl_disconn(mqtt_ctl_t handler)
{
    const char *mqtt_disc_cmd = "AT+QMTDISC=0";
    rt_snprintf(handler->buf, handler->buf_size, "%s", mqtt_conn_cmd);

    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    if (res == 0)
    {
        const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
        return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
    }
    return -1;
}

static int mqtt_ctl_sub(mqtt_ctl_t handler)
{
    const char *mqtt_sub_cmd = "AT+QMTSUB=0,1,/a1mRa3t2xvm/dev_1/user/get,0";
    rt_snprintf(handler->buf, handler->buf_size, "%s", mqtt_conn_cmd);

    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    if (res == 0)
    {
        const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
        return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
    }
    return -1;
}

static int mqtt_ctl_unsub(mqtt_ctl_t handler)
{
    const char *mqtt_sub_cmd = "AT+QMTUNS=0,1,/a1mRa3t2xvm/dev_1/user/get,0";
    rt_snprintf(handler->buf, handler->buf_size, "%s", mqtt_conn_cmd);

    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    if (res == 0)
    {
        const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
        return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
    }
    return -1;
}

static int mqtt_ctl_pubex(mqtt_ctl_t handler, const char *buf, int buf_size)
{
    const char *mqtt_pub_cmd = "AT+QMTPUBEX=0,0,0,1,/a1mRa3t2xvm/dev_1/user/update,";

    rt_snprintf(handler->buf, handler->buf_size, "%s%d", mqtt_pub_cmd, buf_size);

    at_resp_set_info(handler->mqtt_resp, MQTT_RESP_SIZE, 1, MQTT_RESP_TIMEOUT);
    int res = at_exec_cmd(handler->mqtt_resp, handler->buf);
    at_resp_set_info(handler->mqtt_resp, MQTT_RESP_SIZE, 0, MQTT_RESP_TIMEOUT);
    if (res == 0)
    {
        res = at_exec_cmd(handler->mqtt_resp, buf);
        if (res == 0)
        {
            const char *resp_line = at_resp_get_line(handler->mqtt_resp, 2);
            return rt_strncmp(resp_line, "OK", rt_strlen("OK")) == 0 ? 0 : -1;
        }
        else
        {
            return -1;
        }
    }
    return -1;
}

static void urc_stat_func(struct at_client *client, const char *data, rt_size_t size)
{
    rt_kprintf("urc_stat_func\r\n");
}

static void urc_open_func(struct at_client *client, const char *data, rt_size_t size)
{
    rt_kprintf("urc_open_func\r\n");
    my_handler->is_open = 1;
}

static void urc_conn_func(struct at_client *client, const char *data, rt_size_t size)
{
    rt_kprintf("urc_conn_func\r\n");
    my_handler->is_conn = 1;
}

static void urc_pubex_func(struct at_client *client, const char *data, rt_size_t size)
{
    rt_kprintf("urc_pubex_func\r\n");
}

int mqtt_ctl_test(int argc, char **argv)
{
    my_handler = mqtt_ctl_create();
    if (my_handler == NULL)
    {
        return -1;
    }

    if (my_handler->cfg)
    {
        int res = my_handler->cfg(my_handler);
        if (res != 0)
        {
            rt_kprintf("Failed to cfg.\r\n");
        }
        else
        {
            rt_kprintf("Successed to cfg.\r\n");
        }
    }

    if (my_handler->open)
    {
        int res = my_handler->open(my_handler);
        if (res != 0)
        {
            rt_kprintf("Failed to open.\r\n");
        }
        else
        {
            rt_kprintf("Successed to open.\r\n");
        }
    }

    while (!my_handler->is_open)
        ;

    if (my_handler->conn)
    {
        int res = my_handler->conn(my_handler);
        if (res != 0)
        {
            rt_kprintf("Failed to conn.\r\n");
        }
        else
        {
            rt_kprintf("Successed to conn.\r\n");
        }
    }

    while (!my_handler->is_conn)
        ;

    if (my_handler->pubex)
    {
        int res = my_handler->pubex(my_handler, "Hello", rt_strlen("Hello"));
        if (res != 0)
        {
            rt_kprintf("Failed to pub.\r\n");
        }
        else
        {
            rt_kprintf("Successed to pub.\r\n");
        }
    }

    mqtt_ctl_delete(my_handler);
    return 0;
}
#ifdef FINSH_USING_MSH
#include <finsh.h>
/* 输出 at_Client_send 函数到 msh 中 */
MSH_CMD_EXPORT(mqtt_ctl_test, );
#endif
