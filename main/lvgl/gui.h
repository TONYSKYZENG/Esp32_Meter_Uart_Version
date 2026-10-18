#ifndef _LVGL_GUI_H_
#define _LVGL_GUI_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 显示数据发布接口（线程安全）
 *
 * LVGL 不是线程安全的：widget 只能在 LVGL 任务里操作，且共享数据不能一边写一边读。
 * 因此采样任务 / 按键任务都不直接接触 widget，也不让界面直接去读那些测量全局量，
 * 而是调用下面两个函数，把数据写进 gui.c 里一份由临界区（spinlock）保护的快照；
 * LVGL 任务中的刷新定时器再取快照并更新界面。
 * 这样既避免了跨线程调用 LVGL API，也避免了 double 等宽数据类型被“撕裂读”。
 */

/**
 * @brief 采样任务调用：发布一次电压/电流/功率/能耗测量值及已运行时间
 *
 * @param voltage_mv 电压，单位 mV
 * @param current_ma 电流，单位 mA
 * @param power_mw   功率，单位 mW
 * @param energy_mj  累计总能耗，单位 mJ（界面按数值大小自动在 mJ / J 间切换显示）
 * @param elapsed_ms 本次测量已运行时间，单位 ms（未测量时为 0）
 */
void gui_publish_measurements(int32_t voltage_mv, int32_t current_ma,
                              int32_t power_mw, int32_t energy_mj,
                              int32_t elapsed_ms);

/**
 * @brief 按键任务调用：发布测量运行/停止状态（控制状态 LED）
 *
 * @param running true = 正在测量
 */
void gui_publish_running(bool running);

#ifdef __cplusplus
}
#endif

#endif /* _LVGL_GUI_H_ */
