/*
 * adc.c
 *
 *  Created on: 4 июн. 2026 г.
 *      Author: i.novikov
 */


 #include <stdio.h>
 #include <string.h>
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "freertos/queue.h"
 #include "driver/adc.h"
 #include "esp_adc/adc_continuous.h"
 #include "esp_log.h"

 #define TAG "ADC_INT"

 // 🔹 Настройки АЦП для ESP32-S3
 #define ADC_UNIT                ADC_UNIT_1
 #define ADC_CHANNEL             ADC_CHANNEL_6       // GPIO7 на ESP32-S3
 #define ADC_ATTEN               ADC_ATTEN_DB_12     // 12 dB (k≈25%)
 #define ADC_BIT_WIDTH           ADC_BITWIDTH_12     // 12 бит (0–4095)

 // 🔹 Настройки непрерывного режима
 #define SAMPLE_FREQ_HZ          1000                // Частота дискретизации (до 20 кГц стабильно)
 #define BUFFER_SIZE             256                 // Размер буфера в байтах (кратен 4)
 #define CONV_TIMEOUT_MS         100                 // Таймаут ожидания данных

 // 🔹 Период обработки данных в задаче (мс)
 #define PROCESS_PERIOD_MS       100

 // Глобальные дескрипторы
 static adc_continuous_handle_t adc_handle = NULL;
 static QueueHandle_t adc_data_queue = NULL;

 // 📊 Структура для передачи данных из ISR в задачу
 typedef struct {
     uint32_t value;      // Сырое значение АЦП (12 бит)
     uint32_t timestamp;  // Тик времени (для анализа задержек)
 } adc_sample_t;

 // 📊 Паттерн сканирования (массив каналов)
 static const adc_digi_pattern_config_t adc_pattern[] = {
     {
         .unit = ADC_UNIT,
         .channel = ADC_CHANNEL,
         .atten = ADC_ATTEN,
         .bit_width = ADC_BIT_WIDTH,
     },
 };

 

 
 // ⚡ Callback: вызывается в контексте прерывания при готовности данных
 // Должен быть максимально коротким! Только отправка в очередь/флаг.
 static bool IRAM_ATTR adc_callback(adc_continuous_handle_t handle, 
                                    const adc_continuous_evt_data_t *edata, 
                                    void *user_data)
 {
     BaseType_t xHigherPriorityTaskWoken = pdFALSE;
     
     // Отправляем событие в очередь (неблокирующе)
     uint32_t dummy = 1;
     xQueueSendFromISR(adc_data_queue, &dummy, &xHigherPriorityTaskWoken);
     
     return xHigherPriorityTaskWoken == pdTRUE;
 }

 // 🔄 Задача обработки данных из АЦП
 static void adc_process_task(void *pvParameters)
 {
     uint8_t *result = malloc(BUFFER_SIZE);
     if (!result) {
         ESP_LOGE(TAG, "Failed to allocate result buffer");
         vTaskDelete(NULL);
         return;
     }

     adc_continuous_data_t data_cfg ;

	 
     ESP_LOGI(TAG, "ADC processing task started");

     while (1) {
         uint32_t event;
         
         // Ждём сигнал от callback (с таймаутом)
         if (xQueueReceive(adc_data_queue, &event, pdMS_TO_TICKS(CONV_TIMEOUT_MS)) == pdTRUE) {
             
             // Читаем данные из буфера АЦП
             uint32_t ret_num = 0;
             esp_err_t ret = adc_continuous_read(adc_handle, &data_cfg,2, &ret_num, pdMS_TO_TICKS(10));
             
             if (ret == ESP_OK && ret_num > 0) {
                 // Парсим результаты: каждый сэмпл — 4 байта (формат: [channel:4][data:12][reserved:16])
                 for (uint32_t i = 0; i < ret_num; i += 4) {
                     uint32_t sample = result[i] | (result[i+1] << 8) | (result[i+2] << 16) | (result[i+3] << 24);
                     
                     // Извлекаем канал и значение (см. документацию по формату)
                     uint32_t channel = (sample >> 0) & 0x7;      // биты 0-2: номер канала
                     uint32_t value = (sample >> 4) & 0xFFF;      // биты 4-15: 12-битное значение
                     
                     // Фильтруем только наш канал
                     if (channel == ADC_CHANNEL) {
                         // Грубый пересчёт в милливольты (для точности нужна калибровка)
                         uint32_t voltage_mv = value * 3300 / 4095;
                         
                         ESP_LOGI(TAG, "ADC_CH%d: Raw=%4d  Voltage=~%4d mV", 
                                  channel, value, voltage_mv);
                     }
                 }
             } else if (ret != ESP_ERR_TIMEOUT) {
                 ESP_LOGW(TAG, "adc_continuous_read failed: %s", esp_err_to_name(ret));
             }
         }
         
         // Небольшая задержка, чтобы не грузить CPU на 100%
         vTaskDelay(pdMS_TO_TICKS(PROCESS_PERIOD_MS));
     }
     
     free(result);
     vTaskDelete(NULL);
 }

 // 🚀 Основная функция
 void app_main(void)
 {
     ESP_LOGI(TAG, "ADC Interrupt Example for ESP32-S3");

     // ---------- 1. Создаём очередь для событий ----------
     adc_data_queue = xQueueCreate(10, sizeof(uint32_t));
     if (!adc_data_queue) {
         ESP_LOGE(TAG, "Failed to create queue");
         return;
     }

     // ---------- 2. Инициализация непрерывного режима АЦП ----------
     adc_continuous_handle_cfg_t handle_cfg = {
         .max_store_buf_size = 1024,      // Макс. размер внутреннего буфера
         .conv_frame_size = BUFFER_SIZE,  // Размер кадра для чтения
     };
     ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

     // ---------- 3. Конфигурация режима ----------
     adc_continuous_config_t dig_cfg = {
         .sample_freq_hz = SAMPLE_FREQ_HZ,
         .conv_mode = ADC_CONV_SINGLE_UNIT_1,  // Только ADC1
         .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1, // Формат вывода (Type1 для ESP32-S3)
         .pattern_num = 1,                      // Количество каналов в паттерне
         .adc_pattern = adc_pattern,            // Указатель на массив паттернов
     };
     ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

     // ---------- 4. Регистрация callback ----------
     adc_continuous_evt_cbs_t cbs = {
         .on_conv_done = adc_callback,  // Вызывается при заполнении буфера
     };
     ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(adc_handle, &cbs, NULL));

     // ---------- 5. Запуск преобразования ----------
     ESP_ERROR_CHECK(adc_continuous_start(adc_handle));
     ESP_LOGI(TAG, "ADC continuous mode started, sampling at %d Hz", SAMPLE_FREQ_HZ);

     // ---------- 6. Запускаем задачу обработки ----------
     xTaskCreate(adc_process_task, "adc_proc", 4096, NULL, 5, NULL);

     // Главный цикл может заниматься другими делами
     while (1) {
         // Например: обработка Wi-Fi, кнопок, дисплея...
         vTaskDelay(pdMS_TO_TICKS(1000));
         ESP_LOGI(TAG, "System running, ADC sampling in background");
     }
 }