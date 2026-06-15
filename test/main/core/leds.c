/*
 * leds.c
 *
 *  Created on: 2 июн. 2026 г.
 *      Author: i.novikov
 */

 #include "led_strip_types.h"
#include "sdkconfig.h"
 #include "leds.h"
 #include "driver/gpio.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/projdefs.h"
 #include "freertos/event_groups.h"
 #include "freertos/task.h"
 #include "freertos/timers.h"
 #include "led_strip.h"
 #include "esp_log.h"
 #include <stdint.h>
 #include "device_statuses.h"
#include "xtensa/config/specreg.h"

 static const char * TAG = "leds";
 
 void ControlLed_task_function(void *pvParameters);


 TaskHandle_t ControlLed_handler;
 TimerHandle_t rgb_led_TimerHandle = NULL;
 
 static void configure_led(led_strip_handle_t* pled_handle);
 static void set_rgb_color(led_strip_handle_t led_handle, uint8_t red , uint8_t green , uint8_t blue );
 
 
 
 
 static void blink_single_led(void)
 {	
     static uint8_t single_led_target_state = 0;
     gpio_set_level(CONFIG_SINGLE_LED_GPIO, single_led_target_state);
 	single_led_target_state = !single_led_target_state;
 }
 
 
 void leds_gpio_init(void){
	
	 gpio_reset_pin(CONFIG_SINGLE_LED_GPIO);
	
	 gpio_config_t gpos_s_config;
	 gpos_s_config.mode = GPIO_MODE_OUTPUT;
	 gpos_s_config.pull_up_en = false;
	 gpos_s_config.pull_down_en = false;
	 gpos_s_config.intr_type = GPIO_INTR_DISABLE;
	 gpos_s_config.pin_bit_mask  = 1ULL << CONFIG_SINGLE_LED_GPIO;
	 gpio_config(&gpos_s_config);
	
 }
 
 
 
 

 
 void leds_task_init(void){

	xTaskCreate((TaskFunction_t )ControlLed_task_function,
	          (const char*    )"ControlLedTask",
	          (uint16_t       )1024 * 4,
	          (void*          )NULL,
	          (UBaseType_t    )9,
	          (TaskHandle_t*  )&ControlLed_handler); 
			  
			
 }
 
 

 static led_strip_handle_t led_strip_handle;
 void ControlLed_task_function(void *pvParameters){
	TickType_t xLastWakeTime;
	UBaseType_t free_heap;
	LedCommand_t led_param;
	bool blink_flag = false;
	
	configure_led(&led_strip_handle);
	ESP_LOGI(TAG, "Led Strip task start");
	
	
	vTaskDelay(50);
	xLastWakeTime = xTaskGetTickCount();
	
 		for(;;){
			vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(200));
			
			blink_single_led();
			switch(GetDevStatus()){
			case DEV_STATE_INITIALISATION:
				led_update_params(&led_param , 0 , 125 , 0 , LED_EFFECT_BLINK);			
				break;
			case DEV_STATE_WHAITING:
			case DEV_STATE_DATAREADY:
				led_update_params(&led_param ,0 , 125 , 0 , LED_EFFECT_ON);	
				break;
			case DEV_STATE_MEASUREMENT:
				led_update_params(&led_param ,0 , 0 , 125 , LED_EFFECT_BLINK);	
				break;
			case DEV_STATE_WIFI_CONNECTING:
				led_update_params(&led_param ,0 , 0 , 125 , LED_EFFECT_ON);
				break;
			case DEV_STATE_DATA_SENDING:
				led_update_params(&led_param ,214 , 214 , 26 , LED_EFFECT_BLINK);
				break;
			case DEV_STATE_OTA_UPDATING:
				led_update_params(&led_param ,214 , 214 , 26 , LED_EFFECT_ON);
				break;
			case DEV_STATE_ERROR:
				led_update_params(&led_param ,125 , 0 , 0 , LED_EFFECT_BLINK);
				break; 
			}
			
			
			switch(led_param.effect){
			case LED_EFFECT_OFF:
				set_rgb_color(led_strip_handle ,0 , 0 , 0);
				break;
			case LED_EFFECT_BLINK:
					if(!blink_flag){
						led_update_params(&led_param ,0 , 0 , 0 , LED_EFFECT_OFF);
					}
					blink_flag = !blink_flag;
					set_rgb_color(led_strip_handle , led_param.color.red , led_param.color.green , led_param.color.blue );
				break;
			case LED_EFFECT_ON:
					set_rgb_color(led_strip_handle , led_param.color.red , led_param.color.green , led_param.color.blue );
				break; 				
			
			};
			
		
			
			
			
			
			
			
			free_heap = uxTaskGetStackHighWaterMark(NULL);
			if (free_heap < 100 * 4){
				ESP_LOGW(TAG, "Free wifi task HEAP %d byte" , free_heap * sizeof(StackType_t));
			}
			
			
 		
 		
 		}

 	}
	



	static void led_update_params(LedCommand_t* header , uint8_t red ,  uint8_t green ,  uint8_t blue , LED_EFFECT effect ){
		
			header->color.red = red;
			header->color.green = green;
			header->color.blue = blue;
			header->effect = effect;
	}

	static void set_rgb_color(led_strip_handle_t led_handle, uint8_t red , uint8_t green , uint8_t blue ){
		
		
		if (led_handle == NULL) {
			ESP_LOGE(TAG, "led_handle is NULL");
			return;

			} 
		if ((!red) && (!green) && (!blue)){
			led_strip_clear(led_handle);
			return;
		}

		
		led_strip_set_pixel(led_handle, 0, red, green, blue);
		led_strip_refresh(led_handle);		
		
	}
	


	
	
	static void configure_led(led_strip_handle_t * pled_handle)
	{
	    ESP_LOGI(TAG, "Configured to blink addressable LED!");
	    led_strip_config_t strip_config = {
	        .strip_gpio_num = CONFIG_BLINK_GPIO,
	        .max_leds = 1, // at least one LED on board
	    };

	    led_strip_rmt_config_t rmt_config = {
	        .resolution_hz = 10 * 1000 * 1000, // 10MHz
	        .flags.with_dma = false,
	    };
	    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, pled_handle));
	    led_strip_clear(*pled_handle);
	}



	
	
	