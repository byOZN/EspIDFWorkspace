/*
 * device_statuses.c
 *
 *  Created on: 9 июн. 2026 г.
 *      Author: i.novikov
 */
 

#include "device_statuses.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"

static const char * TAG = "DEV_STAT";

EventGroupHandle_t dev_state_event_group; //сделаем на EG с заделом на распараллеливание процессов состояний

void  device_statuses_init(void){
	 ESP_LOGI(TAG , "initialisation");
	 dev_state_event_group = xEventGroupCreate();
	 SetDevStatus(DEV_STATE_INITIALISATION);
	
}


devState_e GetDevStatus(void){
	
	return xEventGroupGetBits(dev_state_event_group);
	
}




void SetDevStatus(devState_e Status){
	
	xEventGroupClearBits(dev_state_event_group ,GetDevStatus()); // очистим биты , так как предполагется что устройство может быть только в одном состоянии единовременно
	xEventGroupSetBits(dev_state_event_group , Status);
	
}


EventGroupHandle_t GetDevStatusEG(void){
	
	return dev_state_event_group;
}