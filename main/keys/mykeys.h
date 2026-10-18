#ifndef _MYKEYS_H_
#define _MYKEYS_H_
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>
#include "esp_system.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "freertos/queue.h"
#define MYKEY1 (16)
#define MYKEY2 (17)
#define MYKEY3 (18)
#define MYKEYS_SEL (1ULL<<MYKEY1)|(1ULL<<MYKEY2)|(1ULL<<MYKEY3)
void mykeys_init(void);

/* 采样定时器周期（ms），仅用于 esp_timer 时基。
 * 能量积分不再依赖该值（见 anaPower()，按实际经过时间积分）。 */
#define METER_SAMPLE_PERIOD_MS  5

/**
 * @brief 累计一个采样周期的能量并更新峰值功率
 *
 * @param elapsed_ms 距上一次采样的实际经过时间（ms），由采样方用 esp_timer_get_time() 测得。
 *                   按真实时间积分，采样抖动或丢 tick 时能量仍然准确。
 */
void anaPower(uint32_t elapsed_ms);
void startMes(void);
void stopMes(void);
void startShow(void);
void stopShow(void);
void key1_isr_handler(void* arg);
void key2_isr_handler(void* arg);
void key3_isr_handler(void* arg);
#endif