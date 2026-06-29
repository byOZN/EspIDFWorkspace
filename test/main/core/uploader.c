/*
 * uploader.c
 *
 * Формирование CSV из буфера ADC и отправка на сервер по HTTP POST.
 *
 * 
 */

#include "uploader.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_err.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char *TAG = "uploader";

/* ─── Внешний API буфера ADC (определён в adc.c) ──────────────────────── */
/* Экспортируем только то, что нужно — снапшот и чтение по индексу.        */
typedef struct {
    uint32_t count;
    uint32_t head;
    bool     full;
} adc_buf_snapshot_t;

extern adc_buf_snapshot_t adc_buffer_snapshot(void);
extern uint16_t           adc_buffer_read_at(const adc_buf_snapshot_t *s, uint32_t index);

/* ─── Вспомогательные константы ─────────────────────────────────────────── */
/* Максимальный размер строки CSV: "4294967295,65535\r\n" ~ 20 байт         */
#define CSV_ROW_MAX_LEN   24
/* Заголовок */
#define CSV_HEADER        "index,value\r\n"
#define CSV_PREVIEW_ROWS  5          /* сколько строк показывать в preview  */

/* ─── Формирование CSV ──────────────────────────────────────────────────── */
static char *build_csv(const adc_buf_snapshot_t *snap, size_t *out_len)
{
    if (snap->count == 0) {
        ESP_LOGW(TAG, "ADC buffer is empty — nothing to build");
        return NULL;
    }

    /* Оцениваем максимальный размер и выделяем память */
    size_t max_size = strlen(CSV_HEADER) +
                      (size_t)snap->count * CSV_ROW_MAX_LEN + 1;

    char *buf = pvPortMalloc(max_size);
    if (!buf) {
        ESP_LOGE(TAG, "No memory for CSV (need %zu bytes)", max_size);
        return NULL;
    }

    /* Заголовок */
    size_t pos = 0;
    pos += snprintf(buf + pos, max_size - pos, CSV_HEADER);

    /* Данные */
    for (uint32_t i = 0; i < snap->count; i++) {
        uint16_t v = adc_buffer_read_at(snap, i);
        pos += snprintf(buf + pos, max_size - pos, "%lu,%u\r\n",
                        (unsigned long)i, v);
    }

    *out_len = pos;
    ESP_LOGI(TAG, "CSV created: %lu samples, %zu bytes",
             (unsigned long)snap->count, pos);

    /* ─── Preview в консоль ─────────────────────────────────────────────── */
    ESP_LOGI(TAG, "--- CSV preview (first rows) ---");
    /* Печатаем первые CSV_PREVIEW_ROWS строк */
    {
        const char *p = buf;
        int lines = 0;
        while (*p && lines < (int)(CSV_PREVIEW_ROWS + 1)) { /* +1 — заголовок */
            const char *nl = strchr(p, '\n');
            if (!nl) break;
            /* Временный буфер для вывода одной строки без '\r\n' */
            int len = (int)(nl - p);
            if (len > 0 && p[len-1] == '\r') len--;
            ESP_LOGI(TAG, "%.*s", len, p);
            p = nl + 1;
            lines++;
        }
    }

    if (snap->count > CSV_PREVIEW_ROWS) {
        ESP_LOGI(TAG, "...");
        /* Последние CSV_PREVIEW_ROWS строк */
        ESP_LOGI(TAG, "--- CSV preview (last rows) ---");
        uint32_t start_idx = (snap->count > CSV_PREVIEW_ROWS)
                             ? snap->count - CSV_PREVIEW_ROWS : 0;
        for (uint32_t i = start_idx; i < snap->count; i++) {
            uint16_t v = adc_buffer_read_at(snap, i);
            ESP_LOGI(TAG, "%lu,%u", (unsigned long)i, v);
        }
    }

    return buf;
}

/* ─── HTTP-клиент ──────────────────────────────────────────────────────── */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_ON_DATA:
        /* Ответ сервера — печатаем для отладки */
        if (evt->data_len > 0) {
            ESP_LOGI(TAG, "Server response: %.*s",
                     evt->data_len, (char *)evt->data);
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

static esp_err_t http_post_csv(const char *csv_buf, size_t csv_len)
{
    esp_http_client_config_t cfg = {
        .url            = CONFIG_UPLOAD_URL,
        .method         = HTTP_METHOD_POST,
        .event_handler  = http_event_handler,
        .timeout_ms     = 10000,
        .buffer_size    = 2048,
        .buffer_size_tx = 2048,
    };

    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    /* Content-Type: text/csv */
    esp_http_client_set_header(client, "Content-Type", "text/csv");

    /* Устанавливаем тело запроса */
    esp_err_t err = esp_http_client_set_post_field(client, csv_buf, (int)csv_len);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "set_post_field failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    ESP_LOGI(TAG, "CSV upload started → %s (%zu bytes)", CONFIG_UPLOAD_URL, csv_len);

    err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "CSV upload completed, HTTP status: %d", status);
        if (status < 200 || status >= 300) {
            ESP_LOGE(TAG, "Server returned non-2xx status: %d", status);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}


esp_err_t uploader_send_csv(void)
{
    /* 1. Снапшот буфера */
    adc_buf_snapshot_t snap = adc_buffer_snapshot();
    ESP_LOGI(TAG, "ADC samples collected: %lu", (unsigned long)snap.count);

    if (snap.count == 0) {
        ESP_LOGE(TAG, "ADC buffer empty — nothing to send");
        return ESP_ERR_INVALID_STATE;
    }

    /* 2. Формируем CSV */
    size_t csv_len = 0;
    char  *csv_buf = build_csv(&snap, &csv_len);
    if (!csv_buf) {
        return ESP_ERR_NO_MEM;
    }

    /* 3. Отправляем */
    esp_err_t err = http_post_csv(csv_buf, csv_len);

    free(csv_buf);
    return err;
}
