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

 
 void ControlLed_task_function(void *pvParameters);
 TaskHandle_t ControlLed_handler;
 
 static void blink_led(void)
 {	
     static uint8_t s_led_state = 0;
     gpio_set_level(CONFIG_BLINK_GPIO, s_led_state);
 	s_led_state = !s_led_state;
 }
 
 
 void leds_gpio_init(void){
	
	 gpio_reset_pin(CONFIG_BLINK_GPIO);
	
	 gpio_config_t gpos_s_config;
	 gpos_s_config.mode = GPIO_MODE_OUTPUT;
	 gpos_s_config.pull_up_en = false;
	 gpos_s_config.pull_down_en = false;
	 gpos_s_config.intr_type = GPIO_INTR_DISABLE;
	 gpos_s_config.pin_bit_mask  = 1ULL << CONFIG_BLINK_GPIO;
	 gpio_config(&gpos_s_config);
	
 }
 
 
 void leds_task_init(void){
	
	
	
	
	xTaskCreate((TaskFunction_t )ControlLed_task_function,
	          (const char*    )"ControlLedTask",
	          (uint16_t       )256 * 4,
	          (void*          )NULL,
	          (UBaseType_t    )9,
	          (TaskHandle_t*  )&ControlLed_handler); 
	
	
 }
 
 
 
 void ControlLed_task_function(void *pvParameters){
 	
 		for(;;){
 			blink_led();
 			vTaskDelay(pdMS_TO_TICKS(200));
 		
 		}

 	}