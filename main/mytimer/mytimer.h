#ifndef _MYTIMER_H_
#define _MYTIMER_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "esp_err.h"
#include "esp_types.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gptimer.h"

/* 定时器索引，用于区分同一定时器组中的两个硬件定时器 */
typedef enum {
    TIMER_0 = 0, /*!< 定时器 0 */
    TIMER_1,     /*!< 定时器 1 */
    TIMER_IDX_MAX,
} mytimer_idx_t;

typedef struct {
    int type;  // the type of timer's event
    int timer_idx;
    uint64_t timer_counter_value;
} timer_event_t;

/**
 * @brief 初始化并启动一个周期定时器
 *
 * @param timer_idx 定时器索引，取值 TIMER_0 或 TIMER_1
 * @param ms        周期，单位毫秒
 *
 * @return
 *     - ESP_OK                 成功
 *     - ESP_ERR_INVALID_ARG    参数错误
 *     - ESP_ERR_INVALID_STATE  该定时器已经初始化
 *     - 其他非 ESP_OK 值        底层 GPTimer 驱动返回的错误码
 */
esp_err_t initTimerx(int timer_idx, double ms);

/**
 * @brief 停止并释放一个已初始化的定时器
 *
 * @param timer_idx 定时器索引，取值 TIMER_0 或 TIMER_1
 *
 * @return
 *     - ESP_OK                 成功
 *     - ESP_ERR_INVALID_ARG    参数错误
 *     - ESP_ERR_INVALID_STATE  该定时器尚未初始化
 */
esp_err_t deinitTimerx(int timer_idx);

void t0Isr(void);
void t1Isr(void);

#endif
