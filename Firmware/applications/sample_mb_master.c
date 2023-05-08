/*
 * Copyright (c) 2006-2022, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2019-06-21     flybreak     first version
 */

#include <rtthread.h>

#include "mb.h"
#include "mb_m.h"
#include "cJSON.h"

#define DBG_TAG "sample_mb_master"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#ifdef PKG_MODBUS_MASTER_SAMPLE
#define SLAVE_ADDR      MB_SAMPLE_TEST_SLAVE_ADDR
#define PORT_NUM        MB_MASTER_USING_PORT_NUM
#define PORT_BAUDRATE   MB_MASTER_USING_PORT_BAUDRATE
#else
#define SLAVE_ADDR      0x01
#define PORT_NUM        3
#define PORT_BAUDRATE   115200
#endif
#define PORT_PARITY     MB_PAR_EVEN

#define MB_POLL_THREAD_PRIORITY  10
#define MB_SEND_THREAD_PRIORITY  RT_THREAD_PRIORITY_MAX - 1

#define MB_SEND_REG_START  2
#define MB_SEND_REG_NUM    2

#define MB_POLL_CYCLE_MS   500

static int msg_proc(int slaveAddr, int func, int regStart, int regNum, int rw);

extern rt_sem_t recv_sem;
extern char msg_buf[256];
static uint8_t data_buf[16];
static uint16_t data2_buf[16];

static void send_thread_entry(void *parameter)
{
    eMBMasterReqErrCode error_code = MB_MRE_NO_ERR;
    rt_uint16_t error_count = 0;

    while (1)
    {
        rt_sem_take(recv_sem, RT_WAITING_FOREVER);
        LOG_D("modbus msg: %s", msg_buf);

        cJSON *json = cJSON_Parse(msg_buf);
        int slaveAddr = cJSON_GetObjectItem(json, "slaveAddr")->valueint;
        int func = cJSON_GetObjectItem(json, "func")->valueint;
        int regStart = cJSON_GetObjectItem(json, "regStart")->valueint;
        int regNum = cJSON_GetObjectItem(json, "regNum")->valueint;
        int rw = cJSON_GetObjectItem(json, "rw")->valueint;

        LOG_D("json data: slaveAddr: %d, func: %d, regStart: %d, regNum: %d, rw: %d.", slaveAddr, func, regStart, regNum, rw);

        switch (func)
        {
        case 1:
            if (rw == 0)
            {
                cJSON *array = cJSON_GetObjectItem(json, "data");
                uint8_t *data = rt_malloc(16 * sizeof(uint8_t));
                if (!data)
                    continue;

                for (int i = 0; i < regNum; i++)
                {
                    data[i] = (uint8_t)cJSON_GetArrayItem(array, i)->valueint;
                    LOG_D("data[%d]: %d", i, data[i]);
                }
                error_code = eMBMasterReqWriteMultipleCoils(slaveAddr, regStart, regNum, data, RT_WAITING_FOREVER);
                rt_free(data);
            }
            else
            {
                error_code = eMBMasterReqReadCoils(slaveAddr, regStart, regNum, RT_WAITING_FOREVER);
            }
            break;
        case 2:
        case 3:
            if (rw == 0)
            {
                cJSON *array = cJSON_GetObjectItem(json, "data");
                uint16_t *data = rt_malloc(16 * sizeof(uint16_t));
                if (!data)
                    continue;

                for (int i = 0; i < regNum; i++)
                {
                    data[i] = (uint16_t)cJSON_GetArrayItem(array, i)->valueint;
                    LOG_D("data[%d]: %d", i, data[i]);
                }
                error_code = eMBMasterReqWriteMultipleHoldingRegister(slaveAddr, regStart, regNum, data, RT_WAITING_FOREVER);
                rt_free(data);
            }
            else
            {
                error_code = eMBMasterReqReadCoils(slaveAddr, regStart, regNum, RT_WAITING_FOREVER);
            }
            break;
        case 4:
        default:
            break;
        }

        /* Record the number of errors */
        if (error_code != MB_MRE_NO_ERR)
        {
            error_count++;
        }

        cJSON_Delete(json);
    }
}

static void mb_master_poll(void *parameter)
{
    eMBMasterInit(MB_RTU, PORT_NUM, PORT_BAUDRATE, PORT_PARITY);
    eMBMasterEnable();

    while (1)
    {
        eMBMasterPoll();
        rt_thread_mdelay(MB_POLL_CYCLE_MS);
    }
}

int mb_master_sample(int argc, char **argv)
{
    static rt_uint8_t is_init = 0;
    rt_thread_t tid1 = RT_NULL, tid2 = RT_NULL;

    if (is_init > 0)
    {
        rt_kprintf("sample is running\n");
        return -RT_ERROR;
    }
    tid1 = rt_thread_create("md_m_poll", mb_master_poll, RT_NULL, 512, MB_POLL_THREAD_PRIORITY, 10);
    if (tid1 != RT_NULL)
    {
        rt_thread_startup(tid1);
    }
    else
    {
        goto __exit;
    }

    tid2 = rt_thread_create("md_m_send", send_thread_entry, RT_NULL, 1024, MB_SEND_THREAD_PRIORITY, 10);
    if (tid2 != RT_NULL)
    {
        rt_thread_startup(tid2);
    }
    else
    {
        goto __exit;
    }

    is_init = 1;
    return RT_EOK;

__exit:
    if (tid1)
        rt_thread_delete(tid1);
    if (tid2)
        rt_thread_delete(tid2);

    return -RT_ERROR;
}
MSH_CMD_EXPORT(mb_master_sample, run a modbus master sample);

static int msg_proc(int slaveAddr, int func, int regStart, int regNum, int rw)
{
    int res = 0;
    switch (func)
    {
    case 1:
        if (rw == 0)
        {
            res = eMBMasterReqWriteMultipleCoils(slaveAddr, regStart, regNum, data_buf, RT_WAITING_FOREVER);
            LOG_D("res: %d", res);
        }
        else
        {
            res = eMBMasterReqReadCoils(slaveAddr, regStart, regNum, RT_WAITING_FOREVER);
        }
        break;
    case 3:
        if (rw == 0)
        {
            res = eMBMasterReqWriteMultipleHoldingRegister(slaveAddr, regStart, regNum, data2_buf, RT_WAITING_FOREVER);
        }
    }
    return 0;
}
