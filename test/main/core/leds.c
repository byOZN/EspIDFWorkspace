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
 #include "led_strip.h"
 #include "esp_log.h"

 static const char * TAG = "leds";
 
 void ControlLed_task_function(void *pvParameters);
 TaskHandle_t ControlLed_handler;
 
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
 
 static void configure_led(void);
 static void blink_led(void);
 static uint8_t s_led_state = 0;
 void ControlLed_task_function(void *pvParameters){
		configure_led();
		
 		for(;;){
 			blink_single_led();
			blink_led();
			s_led_state = !s_led_state;
 			vTaskDelay(pdMS_TO_TICKS(200));
 		
 		}

 	}
	



	static led_strip_handle_t led_strip;

	static void blink_led(void)
	{
	    /* If the addressable LED is enabled */
	    if (s_led_state) {
	        /* Set the LED pixel using RGB from 0 (0%) to 255 (100%) for each color */
	        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
	        /* Refresh the strip to send data */
	        led_strip_refresh(led_strip);
	    } else {
	        /* Set all LED off to clear all pixels */
	        led_strip_clear(led_strip);
	    }
	}

	static void configure_led(void)
	{
	    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
	    /* LED strip initialization with the GPIO and pixels number*/
	    led_strip_config_t strip_config = {
	        .strip_gpio_num = CONFIG_BLINK_GPIO,
	        .max_leds = 1, // at least one LED on board
	    };
	#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
	    led_strip_rmt_config_t rmt_config = {
	        .resolution_hz = 10 * 1000 * 1000, // 10MHz
	        .flags.with_dma = false,
	    };
	    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
	#endif
	    /* Set all LED off to clear all pixels */
	    led_strip_clear(led_strip);
	}


	
	
	