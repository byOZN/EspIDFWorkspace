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


void common_gpio_initialization(void){
	
	
	leds_gpio_init();
	buttons_gpio_init();

	
	
}

void app_main(void)
{

	
	btn_event_t queue_param;


	common_gpio_initialization();
	
	leds_task_init();
	buttons_task_init();
	

	

			  
	

	// NVS 
	esp_err_t ret = nvs_flash_init();
	ESP_LOGI(TAG , "nvs_flash_init: 0x%04x", ret);
	
	//wifi_control_task_init();
	
	
	
    while (1) {
		
		if(xQueueReceive(button_1.queue, &queue_param,portMAX_DELAY) == pdTRUE) {
			
			switch(queue_param){
				case BTN_EVENT_CLICK :
				ESP_LOGI(TAG, "BTN click");
				break;
				case BTN_EVENT_LONG_PRESS :
				ESP_LOGI(TAG, "LONG press");
				break;	
				default:
					break;			
			}
			ESP_LOGI(TAG, "EXTI_");
		}
		
	
//        vTaskDelay(pdMS_TO_TICKS(200));
    }
}






