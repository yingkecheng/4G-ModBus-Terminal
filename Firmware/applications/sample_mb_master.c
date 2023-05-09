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
#include "mqtt_ctl.h"
#include "user_mb_app.h"

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

#define MB_POLL_THREAD_PRIORITY  9
#define MB_SEND_THREAD_PRIORITY  RT_THREAD_PRIORITY_MAX - 1

#define MB_SEND_REG_START  2
#define MB_SEND_REG_NUM    2

#define MB_POLL_CYCLE_MS   500

extern UCHAR    ucMDiscInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_DISCRETE_INPUT_NDISCRETES/8];
extern UCHAR    ucMCoilBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_COIL_NCOILS/8];
extern USHORT   usMRegInBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_INPUT_NREGS];
extern USHORT   usMRegHoldBuf[MB_MASTER_TOTAL_SLAVE_NUM][M_REG_HOLDING_NREGS];

extern rt_sem_t recv_sem;
extern char msg_buf[256];
extern mqtt_ctl_t my_handler;

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
                rt_memset(data, 0, 16 * sizeof(uint8_t));
                if (!data)
                    continue;

                for (int i = 0; i < regNum; i++)
                {
                    uint8_t temp = (uint8_t)cJSON_GetArrayItem(array, i)->valueint;
                    xMBUtilSetBits(data, i, 1, temp);
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
            if (rw == 1)
            {
                error_code = eMBMasterReqReadDiscreteInputs(slaveAddr, regStart, regNum, RT_WAITING_FOREVER);
            }
            else
            {
                error_code = MB_MRE_ILL_ARG;
            }
            break;
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
                error_code = eMBMasterReqReadHoldingRegister(slaveAddr, regStart, regNum, RT_WAITING_FOREVER);
            }
            break;
        case 4:
            if (rw == 1)
            {
                error_code = eMBMasterReqReadInputRegister(slaveAddr, regStart, regNum, RT_WAITING_FOREVER);
            }
            else
            {
                error_code = MB_MRE_ILL_ARG;
            }
            break;
        default:
            break;
        }

        LOG_D("error code: %d", error_code);



        if (error_code == MB_MRE_NO_ERR && rw == 1)
        {
            int *data = NULL;
            cJSON *array = NULL;
            switch (func)
            {
            case 1:
                data = rt_malloc(16 * sizeof(int));
                rt_memset(data, 0, 16 * sizeof(int));
                if (!data)
                    continue;
                for (int i = 0; i < regNum; i++)
                {
                    data[i] = (int)xMBUtilGetBits(ucMCoilBuf[slaveAddr - 1], i + regStart, 1);
                }
                array = cJSON_CreateIntArray(data, regNum);
                cJSON_AddItemToObject(json, "data", array);
                rt_free(data);
                break;
            case 2:
                data = rt_malloc(16 * sizeof(int));
                rt_memset(data, 0, 16 * sizeof(int));
                if (!data)
                    continue;
                for (int i = 0; i < regNum; i++)
                {
                    data[i] = (int)xMBUtilGetBits(ucMDiscInBuf[slaveAddr - 1], i + regStart, 1);
                }
                array = cJSON_CreateIntArray(data, regNum);
                cJSON_AddItemToObject(json, "data", array);
                rt_free(data);
                break;
            case 3:
                data = rt_malloc(16 * sizeof(int));
                rt_memset(data, 0, 16 * sizeof(int));
                if (!data)
                    continue;
                for (int i = 0; i < regNum; i++)
                {
                    data[i] = (int)usMRegHoldBuf[slaveAddr - 1][regStart + i];
                }
                array = cJSON_CreateIntArray(data, regNum);
                cJSON_AddItemToObject(json, "data", array);
                rt_free(data);
                break;
            case 4:
                data = rt_malloc(16 * sizeof(int));
                rt_memset(data, 0, 16 * sizeof(int));
                if (!data)
                    continue;
                for (int i = 0; i < regNum; i++)
                {
                    data[i] = (int)usMRegInBuf[slaveAddr - 1][regStart + i];
                }
                array = cJSON_CreateIntArray(data, regNum);
                cJSON_AddItemToObject(json, "data", array);
                rt_free(data);
                break;
            default:
                break;
            }
        }

        cJSON_AddNumberToObject(json, "result", error_code);
        cJSON_PrintPreallocated(json, msg_buf, 256, 0);
        my_handler->pubex(my_handler, msg_buf, rt_strlen(msg_buf));

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
    eMBErrorCode error_code = MB_ENOERR;
    error_code = eMBMasterInit(MB_RTU, PORT_NUM, PORT_BAUDRATE, PORT_PARITY);
    LOG_D("error code: %d", error_code);

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
