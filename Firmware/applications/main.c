/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-04-24     RT-Thread    first version
 */

#include <rtthread.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include <at_device_ec200x.h>

int main(void)
{
    int count = 1;

    while (count++)
    {
//        rt_kprintf("hello\r\n");
        rt_thread_mdelay(1000);
    }

    return RT_EOK;
}
