/*
 * Copyright (c) 2006-2023, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-05-05     RT-Thread    first version
 */

#include <rtthread.h>

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>

#include "at.h"

int main(void)
{
    int count = 1;

    while (count++)
    {
//        LOG_D("Hello RT-Thread!");
        rt_thread_mdelay(1000);
    }

    return RT_EOK;
}

static int at_demo(int argc, char **argv)
{
    at_client_init("uart2", 128);
    return 0;
}
MSH_CMD_EXPORT(at_demo, )

