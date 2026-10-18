#ifndef _TASK_PRIO_H_
#define _TASK_PRIO_H_

/*
 * 任务优先级（数值越大优先级越高）
 *
 * 约定：串口 > 测量 > 按键 > LED 呼吸 > LVGL 界面。
 *   - 串口收发要求响应最快；
 *   - 测量采样次之，保证采样周期稳定；
 *   - 按键是人工操作，无需最高优先级；
 *   - LVGL 界面只做显示，放在最后，避免抢占上面几个任务。
 */
#define TASK_PRIO_UART      (12)
#define TASK_PRIO_METER     (10)
#define TASK_PRIO_KEYS      (9)
#define TASK_PRIO_LED       (7)
#define TASK_PRIO_LVGL      (4)

#endif /* _TASK_PRIO_H_ */
