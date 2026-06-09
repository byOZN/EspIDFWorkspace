/*
 * buttons.c
 *
 *  Created on: 5 июн. 2026 г.
 *      Author: novik
 */

 
 #include "sdkconfig.h"
 #include "buttons.h"
 #include "esp_log.h"


 TaskHandle_t ButtonsTask_handler;
 QueueHandle_t ButtonsQueue;
 
 static const char * TAG = "leds";
 
 
 void Buttons_task_function(void *pvParameters);

 void buttons_gpio_init(void){

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
	//gpio_isr_handler_add(CONFIG_BUTTON_GPIO, gpio_isr_handle, NULL);
	//gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);

 }
 
 
 btn_state_t button_1 ={
 	.pin = CONFIG_BUTTON_GPIO,
 };
 
 
 
 
 void buttons_task_init(void){

	memset(&button_1,0 , sizeof(btn_state_t));
	button_1.pin = CONFIG_BUTTON_GPIO;
	
	button_1.queue = xQueueCreate(sizeof(uint32_t) , 10);
	if (button_1.queue == NULL) {
		        ESP_LOGE(TAG, "buttons queue not created");
		        return;
	   }
	
	 xTaskCreate((TaskFunction_t )Buttons_task_function,
	           (const char*    )"ButtonsChecker",
	           (uint16_t       )256 * 4,
	           (void*          )&button_1,
	           (UBaseType_t    )2,
	           (TaskHandle_t*  )&ButtonsTask_handler); 
			   
			   

	

			   
}


static void button_poll(btn_state_t *btn);
static inline bool pin_to_logic(gpio_num_t pin) ;
	
void Buttons_task_function(void *pvParameters){
	btn_state_t * button_config = (btn_state_t*)pvParameters;
	for(;;){
	
		
		
		button_poll(button_config);
		
		
		vTaskDelay(pdMS_TO_TICKS(BTN_POLL_PERIOD_MS));
	
	}

}
  


static void button_poll(btn_state_t *btn) {
    const uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    const bool raw_state = pin_to_logic(btn->pin);

    /* ========== Антидребезг ========== */
    if (raw_state != btn->last_raw_state) {
        // Сырое состояние изменилось — сбрасываем таймер
        btn->debounce_tick = now;
        btn->last_raw_state = raw_state;
        return; // Ждём стабилизации
    }

    // Если состояние стабильно меньше DEBOUNCE_MS — выходим
    if ((now - btn->debounce_tick) < BTN_DEBOUNCE_MS) {
        return;
    }

    /* ========== Обработка нажатия/отпускания ========== */
    if (raw_state != btn->stable_state) {
        // Стабильное состояние изменилось
        btn->stable_state = raw_state;

        if (btn->stable_state) {
            // Кнопка НАЖАТА
            btn->press_tick = now;
            btn->long_press_sent = false;
        } else {
            // Кнопка ОТПУЩЕНА
            if (btn->press_tick != 0) {
                const uint32_t duration = now - btn->press_tick;

                if (!btn->long_press_sent && duration < BTN_LONG_PRESS_MS) {
                    // Короткий клик
                    btn_event_t ev = BTN_EVENT_CLICK;
                    xQueueSend(btn->queue, &ev, 0);
                    ESP_LOGD(TAG, "Click: %lu ms", duration);
                }
                btn->press_tick = 0;
            }
        }
    }

    /* ========== Долгое удержание (пока нажато) ========== */
    if (btn->stable_state && !btn->long_press_sent && btn->press_tick != 0) {
        if ((now - btn->press_tick) >= BTN_LONG_PRESS_MS) {
            btn_event_t ev = BTN_EVENT_LONG_PRESS;
            xQueueSend(btn->queue, &ev, 0);
            btn->long_press_sent = true;
            ESP_LOGD(TAG, "Long press");
        }
    }
}

static inline bool pin_to_logic(gpio_num_t pin) {
    int level = gpio_get_level(pin);
    return BTN_ACTIVE_LOW ? (level == 0) : (level == 1);
}
  