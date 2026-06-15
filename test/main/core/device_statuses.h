/*
 * device_statuses.h
 *
 *  Created on: 9 июн. 2026 г.
 *      Author: i.novikov
 */

#ifndef MAIN_CORE_DEVICE_STATUSES_H_
#define MAIN_CORE_DEVICE_STATUSES_H_

#include "esp_bit_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

typedef enum {
    DEV_STATE_INITIALISATION = BIT0,
    DEV_STATE_WHAITING = BIT2,
    DEV_STATE_MEASUREMENT = BIT3,
	DEV_STATE_DATAREADY = BIT4,
    DEV_STATE_WIFI_CONNECTING = BIT5,
	DEV_STATE_DATA_SENDING = BIT6,
	DEV_STATE_OTA_UPDATING = BIT7,
	DEV_STATE_ERROR = BIT8,
} devState_e;


void device_statuses_init(void);

devState_e GetDevStatus(void);
void SetDevStatus(devState_e Status);
EventGroupHandle_t GetDevStatusEG(void);
#endif /* MAIN_CORE_DEVICE_STATUSES_H_ */
