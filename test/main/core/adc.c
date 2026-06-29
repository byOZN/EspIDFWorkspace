/**
 * @file adc.c
 * @brief ESP32 ADC 1 кГц через esp_timer (ESP-IDF v5.x)
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
#include "adc.h"

/* ─── Конфигурация ──────────────────────────────────────────────────────── */
#define ADC_SAMPLE_RATE_HZ   1000
#define ADC_SAMPLE_PERIOD_US (1000000 / ADC_SAMPLE_RATE_HZ)
#define ADC_BUFFER_SIZE      5000

#define MY_ADC_UNIT          ADC_UNIT_1
#define MY_ADC_CHANNEL       ADC_CHANNEL_6
#define MY_ADC_ATTEN         ADC_ATTEN_DB_12
#define MY_ADC_BITWIDTH      ADC_BITWIDTH_12

static const char *TAG = "ADC_BUF";

/* ─── Кольцевой буфер ───────────────────────────────────────────────────── */
static uint16_t          adc_buffer[ADC_BUFFER_SIZE];
static volatile uint32_t buf_head  = 0;
static volatile uint32_t buf_count = 0;
static volatile bool     buf_full  = false;

static portMUX_TYPE buf_mux = portMUX_INITIALIZER_UNLOCKED;

/* ─── ADC хендлы ────────────────────────────────────────────────────────── */
static adc_oneshot_unit_handle_t adc_handle  = NULL;
static adc_cali_handle_t         cali_handle = NULL;
static bool                      cali_ok     = false;

/* ─── Запись в буфер ────────────────────────────────────────────────────── */
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

/* ─── Публичный API буфера (используется uploader.c) ───────────────────── */

/**
 * @brief Снапшот состояния буфера (потокобезопасно).
 */
adc_buf_snapshot_t adc_buffer_snapshot(void)
{
    adc_buf_snapshot_t s;
    portENTER_CRITICAL(&buf_mux);
    s.count = buf_count;
    s.head  = buf_head;
    s.full  = buf_full;
    portEXIT_CRITICAL(&buf_mux);
    return s;
}

/**
 * @brief Чтение значения по логическому индексу (0 = старейшее).
 *        Вызывать только с валидным снапшотом.
 */
uint16_t adc_buffer_read_at(const adc_buf_snapshot_t *s, uint32_t index)
{
    uint32_t oldest = s->full ? s->head : 0;
    return adc_buffer[(oldest + index) % ADC_BUFFER_SIZE];
}

/* ─── Калибровка ────────────────────────────────────────────────────────── */
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
    }
}

/* ─── ADC init ──────────────────────────────────────────────────────────── */
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

/* ─── Таймерный callback (1 кГц) ────────────────────────────────────────── */
static void adc_timer_callback(void *arg)
{
    int raw = 0;
    if (adc_oneshot_read(adc_handle, MY_ADC_CHANNEL, &raw) == ESP_OK) {
        buffer_write((uint16_t)raw);
    }
}

/* ─── Вывод статистики ──────────────────────────────────────────────────── */
static void print_stats(void)
{
    adc_buf_snapshot_t s = adc_buffer_snapshot();

    if (s.count == 0) {
        ESP_LOGI(TAG, "Buffer is empty");
        return;
    }

    uint32_t sum = 0;
    uint16_t mn = UINT16_MAX, mx = 0;

    for (uint32_t i = 0; i < s.count; i++) {
        uint16_t v = adc_buffer_read_at(&s, i);
        if (v < mn) mn = v;
        if (v > mx) mx = v;
        sum += v;
    }
    uint32_t avg = sum / s.count;

    ESP_LOGI(TAG, "ADC samples collected: %lu", (unsigned long)s.count);
    ESP_LOGI(TAG, "RAW  min=%u  max=%u  avg=%lu", mn, mx, (unsigned long)avg);
}

/* ─── Задача статистики/управления измерением ───────────────────────────── */
static void stats_task(void *arg)
{
    esp_timer_handle_t *measurement_timer = (esp_timer_handle_t *)arg;

    while (1) {
        /* Ждём начала измерения */
        xEventGroupWaitBits(GetDevStatusEG(), DEV_STATE_MEASUREMENT,
                            pdFALSE, pdTRUE, portMAX_DELAY);

        /* Сбрасываем буфер перед новым измерением */
        portENTER_CRITICAL(&buf_mux);
        buf_head  = 0;
        buf_count = 0;
        buf_full  = false;
        portEXIT_CRITICAL(&buf_mux);

        ESP_LOGI(TAG, "Measurement started");
        ESP_LOGI(TAG, "ADC sample rate: %d Hz", ADC_SAMPLE_RATE_HZ);
        ESP_LOGI(TAG, "ADC buffer size: %d", ADC_BUFFER_SIZE);

        ESP_ERROR_CHECK(esp_timer_start_periodic(*measurement_timer,
                                                 ADC_SAMPLE_PERIOD_US));

        /* Ждём пока буфер заполнится ИЛИ статус сменится */
        while (1) {
            vTaskDelay(pdMS_TO_TICKS(50));

            devState_e st = GetDevStatus();

            /* Буфер заполнен — завершаем автоматически */
            if (buf_full) {
                ESP_LOGI(TAG, "Buffer full — stopping measurement");
                if (GetDevStatus() == DEV_STATE_MEASUREMENT) {
                    SetDevStatus(DEV_STATE_DATAREADY);
                }
                break;
            }

            /* Состояние сменилось снаружи (кнопка) — выходим */
            if (st != DEV_STATE_MEASUREMENT) {
                break;
            }
        }

        esp_timer_stop(*measurement_timer);
        ESP_LOGI(TAG, "Measurement stopped");

        print_stats();
    }
}

/* ─── Инициализация задачи ──────────────────────────────────────────────── */
void adc_task_init(void)
{
    ESP_LOGI(TAG, "ADC task init. Period: %d us (%d Hz)",
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
