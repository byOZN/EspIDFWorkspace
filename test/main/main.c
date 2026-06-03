/* Blink Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "main.h"
#include "esp_err.h"
#include "esp_wifi_types_generic.h"


#include "core/leds.h"
#include "core/wifi.h"



static const char * TAG = " main.c";






QueueHandle_t ButtonsQueue;

static void IRAM_ATTR gpio_isr_handle(void*arg){
	
	uint32_t gpio_num = (uint32_t) arg;
	
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;	
	xQueueSendFromISR(ButtonsQueue, &gpio_num, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
	
}


void common_gpio_initialization(void){
	
	
	leds_gpio_init();
	
	gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
	
	gpio_reset_pin(CONFIG_BUTTON_GPIO);
	gpio_config_t gpos_s_config;
	
	 gpos_s_config.mode = GPIO_MODE_INPUT;
	 gpos_s_config.pull_up_en = false;
	 gpos_s_config.pull_down_en = false;
	 gpos_s_config.intr_type = GPIO_INTR_POSEDGE;
	 gpos_s_config.mode = GPIO_MODE_INPUT;	 
	 gpos_s_config.pin_bit_mask  = 1ULL << CONFIG_BUTTON_GPIO;
	 gpio_config(&gpos_s_config);


	gpio_set_level(CONFIG_BLINK_GPIO, false);
	gpio_isr_handler_add(CONFIG_BUTTON_GPIO, gpio_isr_handle, NULL);
}

void app_main(void)
{

    /* Configure the peripheral according to the LED type */
	common_gpio_initialization();
	
	leds_task_init();
	
	ButtonsQueue = xQueueCreate(sizeof(uint32_t) , 10);
	

			  
	
	
	uint64_t queue_param;
	
	
	wifi_ap_record_t info;
	// NVS 
	esp_err_t ret = nvs_flash_init();
	ESP_LOGI(TAG , "nvs_flash_init: 0x%04x", ret);
	
	wifi_init_stk();
	
	
	
    while (1) {
       // 
        
		
		
		if(xQueueReceive(ButtonsQueue, &queue_param, 0) == pdTRUE) {
			ESP_LOGI(TAG, "EXTI_");
		}
		
	
		
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}






