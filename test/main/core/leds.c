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
 #include "freertos/task.h"
 #include "freertos/timers.h"
 #include "led_strip.h"
 #include "esp_log.h"
#include <stdint.h>


 static const char * TAG = "leds";
 
 void ControlLed_task_function(void *pvParameters);


 TaskHandle_t ControlLed_handler;
 TimerHandle_t rgb_led_TimerHandle = NULL;
 
 static void configure_led(led_strip_handle_t *led_handle);
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
	UBaseType_t free_heap;

	
	configure_led(&led_strip_handle);
	set_rgb_color(led_strip_handle, 0 , 125, 0 );
	
	ESP_LOGW(TAG, "Led Strip task start");
	vTaskDelay(pdMS_TO_TICKS(1000));
 		for(;;){
 			blink_single_led();
			
			
			free_heap = uxTaskGetStackHighWaterMark(NULL);
			if (free_heap < 100 * 4){
				ESP_LOGW(TAG, "Free wifi task HEAP %d byte" , free_heap * sizeof(StackType_t));
			}
 			vTaskDelay(pdMS_TO_TICKS(200));
 		
 		}

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
	


	
	
	static void configure_led(led_strip_handle_t * led_handle)
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
	    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, led_handle));
	    led_strip_clear(*led_handle);
	}



	
	
	