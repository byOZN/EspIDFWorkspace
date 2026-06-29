/*
 * adc.h
 */

#ifndef MAIN_CORE_ADC_H_
#define MAIN_CORE_ADC_H_

#include <stdint.h>
#include <stdbool.h>

/* ─── Тип снапшота буфера (используется uploader.c) ─────────────────────── */
typedef struct {
    uint32_t count;   /*!< Количество сохранённых отсчётов */
    uint32_t head;    /*!< Индекс следующей записи (голова кольца) */
    bool     full;    /*!< true если буфер был заполнен */
} adc_buf_snapshot_t;

/* ─── Публичные функции модуля ADC ──────────────────────────────────────── */
void adc_init(void);
void adc_task_init(void);

/* Доступ к буферу для uploader.c */
adc_buf_snapshot_t adc_buffer_snapshot(void);
uint16_t           adc_buffer_read_at(const adc_buf_snapshot_t *s, uint32_t index);

#endif /* MAIN_CORE_ADC_H_ */
