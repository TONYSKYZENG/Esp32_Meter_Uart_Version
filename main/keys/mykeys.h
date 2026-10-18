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

/* 采样周期（ms）。采样定时器与 anaPower() 的能量积分共用该值：
 * 修改这里即可整体调整采样周期。注意 FreeRTOS tick 默认 100 Hz（10 ms），
 * 因此周期小于 10 ms 时不能用 vTaskDelay 实现，需用 esp_timer 作为时基。 */
#define METER_SAMPLE_PERIOD_MS  5

void anaPower(void);
void startMes(void);
void stopMes(void);
void startShow(void);
void stopShow(void);
void key1_isr_handler(void* arg);
void key2_isr_handler(void* arg);
void key3_isr_handler(void* arg);
#endif