/*
 * uploader.h
 *
 * Формирование CSV из буфера ADC и отправка на сервер по HTTP POST.
 */

#ifndef MAIN_CORE_UPLOADER_H_
#define MAIN_CORE_UPLOADER_H_

#include "esp_err.h"

/**
 * @brief Сформировать CSV из буфера ADC и отправить на сервер.
 *
 * URL берётся из CONFIG_UPLOAD_URL (Kconfig → "Server Config").
 *
 * @return ESP_OK при успехе, иначе код ошибки ESP-IDF.
 */
esp_err_t uploader_send_csv(void);

#endif /* MAIN_CORE_UPLOADER_H_ */
