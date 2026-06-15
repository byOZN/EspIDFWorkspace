/*
 * leds.h
 *
 *  Created on: 2 июн. 2026 г.
 *      Author: i.novikov
 */

#ifndef MAIN_CORE_LEDS_H_
#define MAIN_CORE_LEDS_H_
#include "stdint.h"

void leds_gpio_init(void);
void leds_task_init(void);


typedef enum {
	LED_EFFECT_OFF,
	LED_EFFECT_BLINK,
	LED_EFFECT_ON	
}LED_EFFECT;

struct LedColor_s {
	uint8_t red;
	uint8_t blue;
	uint8_t green;	
};


typedef struct LedCommand_s {
    LED_EFFECT  effect;
	struct LedColor_s color;
} LedCommand_t ;



static void led_update_params(LedCommand_t* header , uint8_t red ,  uint8_t green ,  uint8_t blue , LED_EFFECT effect );
		

#endif /* MAIN_CORE_LEDS_H_ */
