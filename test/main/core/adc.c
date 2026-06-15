/**
 * @file main.c
 * @brief ESP32 ADC 1 кГц через esp_timer (ESP-IDF v5.x)
 *
 * Исправление: мьютекс заменён на portMUX_TYPE (spinlock).
 * Spinlock корректно работает как из esp_timer callback,
 * так и из обычных задач FreeRTOS.
 */

#include <stdio.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "device_statuses.h"
#include "portmacro.h"

/* ─── Конфигурация ────────────────────────────────────────────────────── */
#define ADC_SAMPLE_RATE_HZ   1000
#define ADC_SAMPLE_PERIOD_US (1000000 / ADC_SAMPLE_RATE_HZ)  // 1000 мкс
#define ADC_BUFFER_SIZE      10000

#define MY_ADC_UNIT          ADC_UNIT_1
#define MY_ADC_CHANNEL       ADC_CHANNEL_6   // GPIO34
#define MY_ADC_ATTEN         ADC_ATTEN_DB_12
#define MY_ADC_BITWIDTH      ADC_BITWIDTH_12

static const char *TAG = "ADC_BUF";

/* ─── Кольцевой буфер ─────────────────────────────────────────────────── */
static uint16_t          adc_buffer[ADC_BUFFER_SIZE];
static volatile uint32_t buf_head  = 0;
static volatile uint32_t buf_count = 0;
static volatile bool     buf_full  = false;

// Spinlock — работает везде: в задачах, таймерах, ISR
static portMUX_TYPE buf_mux = portMUX_INITIALIZER_UNLOCKED;

/* ─── ADC хендлы ──────────────────────────────────────────────────────── */
static adc_oneshot_unit_handle_t adc_handle  = NULL;
static adc_cali_handle_t         cali_handle = NULL;
static bool                      cali_ok     = false;

/* ═══════════════════════════════════════════════════════════════════════ */
/*  Буфер (spinlock защита)                                                */
/* ═══════════════════════════════════════════════════════════════════════ */
static inline void buffer_write(uint16_t value)
{
    portENTER_CRITICAL(&buf_mux);

    adc_buffer[buf_head] = value;
    buf_head = (buf_head + 1) % ADC_BUFFER_SIZE;
    if (buf_count < ADC_BUFFER_SIZE) {
        buf_count++;
    } else {
        buf_full = true;
    }

    portEXIT_CRITICAL(&buf_mux);
}

/* Безопасное чтение снапшота состояния буфера */
typedef struct {
    uint32_t count;
    uint32_t head;
    bool     full;
} buf_snapshot_t;

static buf_snapshot_t buffer_snapshot(void)
{
    buf_snapshot_t s;
    portENTER_CRITICAL(&buf_mux);
    s.count = buf_count;
    s.head  = buf_head;
    s.full  = buf_full;
    portEXIT_CRITICAL(&buf_mux);
    return s;
}

/* Чтение значения по логическому индексу (0 = старейшее) */
static uint16_t buffer_read_at(const buf_snapshot_t *s, uint32_t index)
{
    uint32_t oldest = s->full ? s->head : 0;
    return adc_buffer[(oldest + index) % ADC_BUFFER_SIZE];

}

/* ═══════════════════════════════════════════════════════════════════════ */
/*  Калибровка                                                             */
/* ═══════════════════════════════════════════════════════════════════════ */
static void adc_calibration_init(void)
{

    adc_cali_curve_fitting_config_t cfg = {
        .unit_id  = MY_ADC_UNIT,
        .chan     = MY_ADC_CHANNEL,
        .atten    = MY_ADC_ATTEN,
        .bitwidth = MY_ADC_BITWIDTH,
    };
    if (adc_cali_create_scheme_curve_fitting(&cfg, &cali_handle) == ESP_OK) {
        cali_ok = true;
        ESP_LOGI(TAG, "Калибровка: Curve Fitting");
        return;
    }

}

/* ═══════════════════════════════════════════════════════════════════════ */
/*  ADC init                                                               */
/* ═══════════════════════════════════════════════════════════════════════ */
 void adc_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = { .unit_id = MY_ADC_UNIT };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten    = MY_ADC_ATTEN,
        .bitwidth = MY_ADC_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, MY_ADC_CHANNEL, &chan_cfg));

    adc_calibration_init();
}


static void adc_timer_callback(void *arg)
{
    int raw = 0;
    if (adc_oneshot_read(adc_handle, MY_ADC_CHANNEL, &raw) == ESP_OK) {
        buffer_write((uint16_t)raw);
    }
}

/* ═══════════════════════════════════════════════════════════════════════ */
/*  Статистика                                                             */
/* ═══════════════════════════════════════════════════════════════════════ */
static void print_stats(void)
{

    // Берём снапшот — критическая секция минимальна
    buf_snapshot_t s = buffer_snapshot();

    if (s.count == 0) {
        ESP_LOGI(TAG, "Buff is empty");
        return;
    }

    uint32_t sum = 0;
    uint16_t mn = UINT16_MAX, mx = 0;

    for (uint32_t i = 0; i < s.count; i++) {
        uint16_t v = buffer_read_at(&s, i);
		
		if((i < 3) || (i> s.count-3)){
			ESP_LOGI(TAG, "%lu , %u", i, v);
		}
		
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
    }
    uint32_t avg = sum / s.count;


    ESP_LOGI(TAG, "RAW  min=%u  max=%u  avg=%lu",
             mn, mx, (unsigned long)avg);
			 
			 

}

static void stats_task(void *arg)
{
	esp_timer_handle_t* Measurmenttimer = arg;
    while (1) {
		
		// ждем начала измерения
		xEventGroupWaitBits(GetDevStatusEG(), DEV_STATE_MEASUREMENT, pdFALSE, pdTRUE, portMAX_DELAY); 
		ESP_ERROR_CHECK(esp_timer_start_periodic(*Measurmenttimer, ADC_SAMPLE_PERIOD_US));
		ESP_LOGI(TAG, "Starting Measurment");
		
		while(buf_full != true){
			vTaskDelay(pdMS_TO_TICKS(10));
		}
				
		SetDevStatus(DEV_STATE_DATAREADY);
        print_stats();
    }
}

/* ═══════════════════════════════════════════════════════════════════════ */
/*  app_main                                                               */
/* ═══════════════════════════════════════════════════════════════════════ */
void adc_task_init(void)
{
    ESP_LOGI(TAG, "Старт. Период: %d мкс (%d Гц)",
             ADC_SAMPLE_PERIOD_US, ADC_SAMPLE_RATE_HZ);


    static esp_timer_handle_t timer;
    const esp_timer_create_args_t timer_args = {
        .callback              = adc_timer_callback,
        .arg                   = NULL,
        .dispatch_method       = ESP_TIMER_TASK,
        .name                  = "adc_timer",
        .skip_unhandled_events = true,
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
   
    xTaskCreate(stats_task, "adc_stat", 4096, &timer, 5, NULL);
}