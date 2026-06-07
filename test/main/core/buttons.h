/*
 * buttons.h
 *
 *  Created on: 5 июн. 2026 г.
 *      Author: novik
 */

#ifndef MAIN_CORE_BUTTONS_H_
#define MAIN_CORE_BUTTONS_H_

#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"


/* ========= Настройки ========= */
#define BTN_DEBOUNCE_MS      50     // Антидребезг
#define BTN_LONG_PRESS_MS    1000   // Порог долгого удержания
#define BTN_POLL_PERIOD_MS   10     // Период опроса кнопки
#define BTN_ACTIVE_LOW       false   // true - кнопка замыкается на GND
#define BTN_QUEUE_LENGTH     10     // Длина очереди событий

/* ========= События кнопки ========= */
typedef enum {
    BTN_EVENT_NONE = 0,
    BTN_EVENT_CLICK,         // Короткий клик
    BTN_EVENT_LONG_PRESS     // Долгое удержание
} btn_event_t;


typedef struct {
    gpio_num_t pin;              // Номер пина
    uint32_t debounce_tick;      // Время последней смены сырого состояния
    uint32_t press_tick;         // Время, когда кнопка была нажата (после антидребезга)
    bool stable_state;           // Стабильное состояние (после антидребезга)
    bool last_raw_state;         // Последнее сырое состояние
    bool long_press_sent;        // Флаг: long press уже отправлен в очередь
    QueueHandle_t queue;         // Очередь FreeRTOS
} btn_state_t;


typedef struct button_init_s {
	gpio_num_t gpio_number;
	QueueHandle_t* out_queue;
	
} button_init_t;



void buttons_gpio_init(void);
void buttons_task_init(void);


extern  btn_state_t button_1;

#endif /* MAIN_CORE_BUTTONS_H_ */
