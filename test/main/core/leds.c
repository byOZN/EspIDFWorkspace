/*
 * leds.c
 *
 *  Created on: 2 июн. 2026 г.
 *      Author: i.novikov
 */

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
 void rgb_timer_callback(TimerHandle_t xTimer);

 TaskHandle_t ControlLed_handler;
 TimerHandle_t rgb_led_TimerHandle = NULL;
 
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
 
 
 
 
 void set_rgb_color(uint32_t rgb_mask);
 
 void leds_task_init(void){

	xTaskCreate((TaskFunction_t )ControlLed_task_function,
	          (const char*    )"ControlLedTask",
	          (uint16_t       )1024 * 4,
	          (void*          )NULL,
	          (UBaseType_t    )9,
	          (TaskHandle_t*  )&ControlLed_handler); 
			  
			  
	  rgb_led_TimerHandle = xTimerCreate(
			      "RGBTimer",         
			      pdMS_TO_TICKS(10),   
			      pdTRUE,               
			      (void *)1,            
			      rgb_timer_callback       
			  );
	  if (rgb_led_TimerHandle == NULL) {
	        ESP_LOGE(TAG, "Rgb timer create fail");
	        return;
	   }
		
		
	if (xTimerStart(rgb_led_TimerHandle, 0) == pdPASS) {
	    ESP_LOGI(TAG, "Rgb timer run");
	} else {
	    ESP_LOGE(TAG, "Rgb timer not  run");
	}
 }
 
 
 
 static void configure_led(void);
 void ControlLed_task_function(void *pvParameters){
	UBaseType_t free_heap;
	configure_led();
	
	set_rgb_color(25 << 16);
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
	



	static led_strip_handle_t led_strip;

	void set_rgb_color(uint32_t rgb_mask){
		
		rgb_mask = rgb_mask & 0x00FFFFFF;

		if (!rgb_mask){
			led_strip_clear(led_strip);
			return;
		}
		uint32_t red = (rgb_mask >> 16) & 0xFF;
		uint32_t green = (rgb_mask >> 8) & 0xFF;
		uint32_t blue = (rgb_mask >> 0) & 0xFF;
		
		led_strip_set_pixel(led_strip, 0, red, green, blue);
		led_strip_refresh(led_strip);		
		
	}
	


	
	
	static void configure_led(void)
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
	    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
	    led_strip_clear(led_strip);
	}


	static uint32_t rgb_mask  = 0UL;
	
	void rgb_timer_callback(TimerHandle_t xTimer) {
		uint8_t offset =  (rgb_mask >> 24) & 0x7F;
		
		
		if( ((rgb_mask >> ( 8 * offset)) & 0xFF) == 0xFF ){	
			rgb_mask |= (1<<31); //направление
		} else if(((rgb_mask >> ( 8 * offset)) & 0xFF) == 0x00){ 
			rgb_mask &= ~(1<<31);
			
			if(offset == 2){
				rgb_mask = 0;
			} else {
				rgb_mask += 1 << 24;
			}
			offset =  (rgb_mask >> 24) & 0x7F;
			
		}
		
		
		if(rgb_mask >> 31){ //надо уменьшать 
			rgb_mask -=  1 << ( 8 * offset);
		} else {
			rgb_mask +=  1 << ( 8 * offset);
		}
		
		
		set_rgb_color(rgb_mask);
		
		
		//rgb_mask == rgb_mask     1 << ( 8 * offset);
		
			
	}
	
	
	