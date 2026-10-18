/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "mytimer.h"

#include <inttypes.h>

#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "mytimer";

/* GPTimer 计数分辨率：1 MHz，即 1 个计数 = 1 us */
#define MYTIMER_RESOLUTION_HZ   1000000

/* 每个软件定时器索引对应一个 GPTimer 句柄 */
static gptimer_handle_t s_timer_handle[TIMER_IDX_MAX] = { NULL };

/**
 * @brief 定时器报警中断回调，在 GPTimer 的 ISR 上下文中执行
 *
 * @note 回调返回值表示是否唤醒了更高优先级的任务
 */
static bool IRAM_ATTR timer_on_alarm_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    const int timer_idx = (int)(intptr_t)user_ctx;

    if (timer_idx == TIMER_0) {
        t0Isr();
    } else if (timer_idx == TIMER_1) {
        t1Isr();
    }

    /* 回调只置位标志，没有唤醒任务，因此返回 false */
    return false;
}

esp_err_t initTimerx(int timer_idx, double ms)
{
    ESP_RETURN_ON_FALSE(timer_idx >= 0 && timer_idx < TIMER_IDX_MAX, ESP_ERR_INVALID_ARG,
                        TAG, "invalid timer index: %d", timer_idx);
    ESP_RETURN_ON_FALSE(s_timer_handle[timer_idx] == NULL, ESP_ERR_INVALID_STATE,
                        TAG, "timer %d already initialized", timer_idx);

    /* ms 必须大于 0，且换算后的计数不能为 0（分辨率为 1 us，最小周期 1 us） */
    const double ticks = ms * (MYTIMER_RESOLUTION_HZ / 1000.0);
    ESP_RETURN_ON_FALSE(ticks >= 1.0, ESP_ERR_INVALID_ARG,
                        TAG, "invalid alarm interval: %f ms", ms);

    esp_err_t ret = ESP_OK;
    gptimer_handle_t gptimer = NULL;
    bool enabled = false;
    bool started = false;

    /* 通用定时器基本配置：使用默认时钟源，向上计数，1 us 分辨率 */
    const gptimer_config_t timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = MYTIMER_RESOLUTION_HZ,
    };
    ESP_GOTO_ON_ERROR(gptimer_new_timer(&timer_config, &gptimer), err, TAG,
                      "create timer %d failed", timer_idx);

    /* 注册报警事件回调，user_ctx 传递定时器索引用于区分 TIMER_0/TIMER_1 */
    const gptimer_event_callbacks_t cbs = {
        .on_alarm = timer_on_alarm_cb,
    };
    ESP_GOTO_ON_ERROR(gptimer_register_event_callbacks(gptimer, &cbs, (void *)(intptr_t)timer_idx), err, TAG,
                      "register callbacks for timer %d failed", timer_idx);

    /* 报警配置：计数值到 alarm_count 触发中断，并自动重载 */
    const gptimer_alarm_config_t alarm_config = {
        .reload_count = 0,
        .alarm_count = (uint64_t)ticks,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_GOTO_ON_ERROR(gptimer_set_alarm_action(gptimer, &alarm_config), err, TAG,
                      "set alarm action for timer %d failed", timer_idx);

    ESP_GOTO_ON_ERROR(gptimer_enable(gptimer), err, TAG, "enable timer %d failed", timer_idx);
    enabled = true;

    ESP_GOTO_ON_ERROR(gptimer_start(gptimer), err, TAG, "start timer %d failed", timer_idx);
    started = true;

    s_timer_handle[timer_idx] = gptimer;
    ESP_LOGI(TAG, "timer %d started, alarm interval %.3f ms (%" PRIu64 " ticks)",
             timer_idx, ms, (uint64_t)ticks);
    return ESP_OK;

err:
    if (gptimer) {
        if (started) {
            gptimer_stop(gptimer);
        }
        if (enabled) {
            gptimer_disable(gptimer);
        }
        gptimer_del_timer(gptimer);
    }
    return ret;
}

esp_err_t deinitTimerx(int timer_idx)
{
    ESP_RETURN_ON_FALSE(timer_idx >= 0 && timer_idx < TIMER_IDX_MAX, ESP_ERR_INVALID_ARG,
                        TAG, "invalid timer index: %d", timer_idx);
    ESP_RETURN_ON_FALSE(s_timer_handle[timer_idx] != NULL, ESP_ERR_INVALID_STATE,
                        TAG, "timer %d is not initialized", timer_idx);

    gptimer_handle_t gptimer = s_timer_handle[timer_idx];
    s_timer_handle[timer_idx] = NULL;

    ESP_RETURN_ON_ERROR(gptimer_stop(gptimer), TAG, "stop timer %d failed", timer_idx);
    ESP_RETURN_ON_ERROR(gptimer_disable(gptimer), TAG, "disable timer %d failed", timer_idx);
    ESP_RETURN_ON_ERROR(gptimer_del_timer(gptimer), TAG, "delete timer %d failed", timer_idx);

    return ESP_OK;
}
