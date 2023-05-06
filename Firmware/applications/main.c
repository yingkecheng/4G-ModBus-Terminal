/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-04-29     RT-Thread    first version
 */

#include <rtthread.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include <string.h>

#include "mqtt_ctl.h"



int mqtt_ctl_test(int argc, char **argv);
extern int mb_master_sample(int argc, char **argv);

mqtt_ctl_t my_handler = NULL;
char msg_buf[256];
rt_sem_t recv_sem = NULL;

int main(void)
{
//    /* 初始化信号量 */
//    recv_sem = rt_sem_create("recv_sem", 0, RT_IPC_FLAG_PRIO);
//
//    /* 初始化 freemodbus */
//    mb_master_sample(0, NULL);

    /* 运行 mqtt_ctl */
    mqtt_ctl_test(0, NULL);

    /* 不会运行至此处 */
    while (1)
    {
        rt_thread_mdelay(1000);
    }

    return RT_EOK;
}

int mqtt_ctl_test(int argc, char **argv)
{
    my_handler = mqtt_ctl_create();
    if (my_handler == NULL)
    {
        return -1;
    }

    mqtt_ctl_wait_rdy(my_handler);

    if (my_handler->cfg)
    {
        int res = my_handler->cfg(my_handler);
        if (res != 0)
        {
            rt_kprintf("Failed to cfg.\r\n");
            return -1;
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
            return -1;
        }
        else
        {
            rt_kprintf("Successed to open.\r\n");
        }
    }

    while (!my_handler->is_open)
    {
        rt_thread_delay(100);
    }

    if (my_handler->conn)
    {
        int res = my_handler->conn(my_handler);
        if (res != 0)
        {
            rt_kprintf("Failed to conn.\r\n");
            return -1;
        }
        else
        {
            rt_kprintf("Successed to conn.\r\n");
        }
    }

    while (!my_handler->is_conn)
    {
        rt_thread_delay(100);
    }
//
//    if (my_handler->sub)
//    {
//        int res = my_handler->sub(my_handler);
//        if (res != 0)
//        {
//            rt_kprintf("Failed to sub.\r\n");
//        }
//        else
//        {
//            rt_kprintf("Successed to sub.\r\n");
//        }
//    }
//
//    while (1)
//    {
//        rt_thread_delay(100);
//    }
//
//    if (my_handler->unsub)
//    {
//        int res = my_handler->unsub(my_handler);
//        if (res != 0)
//        {
//            rt_kprintf("Failed to unsub.\r\n");
//        }
//        else
//        {
//            rt_kprintf("Successed to unsub.\r\n");
//        }
//    }
//
//    if (my_handler->disconn)
//    {
//        int res = my_handler->disconn(my_handler);
//        if (res != 0)
//        {
//            rt_kprintf("Failed to disconn.\r\n");
//        }
//        else
//        {
//            rt_kprintf("Successed to disconn.\r\n");
//        }
//    }

    mqtt_ctl_delete(my_handler);
    return 0;
}
#ifdef FINSH_USING_MSH
#include <finsh.h>
/* 输出 at_Client_send 函数到 msh 中 */
MSH_CMD_EXPORT(mqtt_ctl_test, );
#endif


