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
 * @brief 累计一个采样周期的能量并更新峰值功率（线程安全）
 *
 * 由采样任务调用。瞬时测量值作为参数传入，避免共享全局量；累计能量、峰值
 * 等跨线程共享的状态在内部用互斥锁保护。
 *
 * @param elapsed_ms 距上一次采样的实际经过时间（ms），由采样方用 esp_timer_get_time() 测得。
 *                   按真实时间积分，采样抖动或丢 tick 时能量仍然准确。
 * @param current_ma 瞬时电流，单位 mA
 * @param voltage_mv 瞬时电压，单位 mV
 * @param power_mw   瞬时功率，单位 mW（用于峰值判断）
 */
void anaPower(uint32_t elapsed_ms, double current_ma, double voltage_mv, double power_mw);

/** @brief 取累计总能耗，单位 J（线程安全） */
double mes_getEnergy(void);

/** @brief 取峰值功率，单位 mW（线程安全） */
double mes_getPeak(void);

/** @brief 取本次测量已运行时间，单位 ms（线程安全）；未在测量时返回 0 */
int32_t mes_getRunMs(void);

void startMes(void);
void stopMes(void);
void startShow(void);
void stopShow(void);
void key1_isr_handler(void* arg);
void key2_isr_handler(void* arg);
void key3_isr_handler(void* arg);
#endif