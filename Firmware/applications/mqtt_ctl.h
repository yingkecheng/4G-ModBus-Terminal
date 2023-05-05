/*
 * Copyright (c) 2006-2021, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2023-04-29     David       the first version
 */
#ifndef APPLICATIONS_MQTT_CTL_H_
#define APPLICATIONS_MQTT_CTL_H_

#include "at.h"

typedef struct mqtt_ctl *mqtt_ctl_t;

struct mqtt_ctl
{
    at_response_t mqtt_resp;               // MQTT response object
    char *buf;                             // Buffer for storing MQTT commands and responses
    int buf_size;                          // Size of the buffer
    int is_open;
    int is_conn;
    int (*cfg)(mqtt_ctl_t handler);        // Function pointer for MQTT configuration
    int (*open)(mqtt_ctl_t handler);       // Function pointer for opening MQTT connection
    int (*conn)(mqtt_ctl_t handler);
    int (*pubex)(mqtt_ctl_t handler, const char *buf, int buf_size);
};


int mqtt_ctl_init(mqtt_ctl_t handler);
void mqtt_ctl_deinit(mqtt_ctl_t handler);
mqtt_ctl_t mqtt_ctl_create(void);
void mqtt_ctl_delete(mqtt_ctl_t handler);

#endif /* APPLICATIONS_MQTT_CTL_H_ */
