/*
 * wifi.h
 *
 *  Created on: 3 июн. 2026 г.
 *      Author: i.novikov
 */

#ifndef MAIN_CORE_WIFI_H_
#define MAIN_CORE_WIFI_H_

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"


void wifi_control_task_init(void);


#endif /* MAIN_CORE_WIFI_H_ */
