/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "main.h"
#include "esp_err.h"
#include "esp_log.h"

#include "core/leds.h"
#include "core/wifi.h"
#include "core/adc.h"
#include "core/buttons.h"
#include "core/device_statuses.h"
#include "freertos/idf_additions.h"
#include "portmacro.h"
#include "rom/opi_flash.h"



static const char * TAG = "main";








//static void IRAM_ATTR gpio_isr_handle(void*arg){
//	
//	uint32_t gpio_num = (uint32_t) arg;
//	
//	BaseType_t xHigherPriorityTaskWoken = pdFALSE;	
//	xQueueSendFromISR(ButtonsQueue, &gpio_num, &xHigherPriorityTaskWoken);
//	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
//	
//}


static void gpio_init(void){
	
	
	leds_gpio_init();
	buttons_gpio_init();

	
	
}

void app_main(void)
{

	//Periph init
	gpio_init();
	adc_init();
	
	
	device_statuses_init(); //инцализируем стутусы устройства 
	leds_task_init();	//запускаем задачу светодиода 
	adc_task_init();

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	

	buttons_task_init();
	vTaskDelay(pdMS_TO_TICKS(2000)); // Эмулируем первичную инициализацию 
	SetDevStatus(DEV_STATE_WHAITING);
	// NVS 
	esp_err_t ret = nvs_flash_init();
	ESP_LOGI(TAG , "nvs_flash_init: 0x%04x", ret);

	wifi_control_task_init();
	
	
	
	btn_event_t queue_param;
	xQueueReset(button_1.queue);

	while (1) {
		
		if(xQueueReceive(button_1.queue, &queue_param,portMAX_DELAY) == pdTRUE) {
			
			switch(queue_param){
				case BTN_EVENT_CLICK :
				if(GetDevStatus() == DEV_STATE_WHAITING){
					SetDevStatus(DEV_STATE_MEASUREMENT);
				} else if(GetDevStatus() == DEV_STATE_DATAREADY){
					SetDevStatus(DEV_STATE_WIFI_CONNECTING);
				}
				break;
				case BTN_EVENT_LONG_PRESS :
				ESP_LOGI(TAG, "LONG press");
				break;	
				default:
					break;			
			}
		}
		
	
//        vTaskDelay(pdMS_TO_TICKS(200));
    }
}






