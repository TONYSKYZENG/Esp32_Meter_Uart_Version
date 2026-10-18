/* LVGL Example project
 *
 * Basic project to test LVGL on ESP32 based projects.
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_freertos_hooks.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "ina226.h"
#include "mykeys.h"
#include "uart0.h"
#include "gui.h"
#include "task_prio.h"
uint16_t g_runTime=0;
uint16_t g_isrTime=0;
uint16_t g_isrTimeP=0;
uint8_t g_isTxData=0;
uint16_t g_ly68Id=0;

/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define LV_TICK_PERIOD_MS 1

/* 采样周期由 METER_SAMPLE_PERIOD_MS 定义（见 mykeys.h），
 * 采样定时器与 anaPower() 的能量积分共用该值 */
#define METER_RUNTIME_DIVIDER   200      /* 200 个采样周期 = 1 s，用于统计运行时间 */
#define METER_TASK_STACK_SIZE   4096

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void meter_task(void *arg);
static void meter_timer_cb(void *arg);
static void meter_init(void);

/**********************
 *  MEASUREMENT UPDATE
 **********************/
static TaskHandle_t s_meter_task = NULL;
static esp_timer_handle_t s_meter_timer = NULL;
extern void app_main_lvgl_drv(void);
/* 定时器回调运行在 esp_timer 任务上下文里：只发一个任务通知就返回，
 * 绝不在这里做 I2C 等阻塞操作（否则会拖垮 esp_timer 任务、破坏时基）。 */
static void meter_timer_cb(void *arg)
{
    (void)arg;

    if (s_meter_task != NULL) {
        xTaskNotifyGive(s_meter_task);
    }
}

/* 采样任务：每次通知采样一次，阻塞式 I2C 读取放在任务里完成。
 * 能量按两次采样之间的实际经过时间积分，因此丢 tick / 调度抖动不会累计误差。 */
static void meter_task(void *arg)
{
    (void)arg;

    int64_t last_sample_us = esp_timer_get_time();

    for (;;) {
        /* 阻塞等待周期通知；多次通知合并为一次采样，用 Δt 覆盖整段时间 */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        const int64_t now_us = esp_timer_get_time();
        const uint32_t elapsed_ms = (uint32_t)((now_us - last_sample_us) / 1000);
        last_sample_us = now_us;

        /* 瞬时测量值只用任务内的局部变量，不再做跨线程共享的全局量 */
        const double v1 = ina226_getV(1);
        const double i1 = ina226_getI(1);
        const double w1 = ina226_getW(1);

        anaPower(elapsed_ms, i1, v1, w1);

        /* 通过线程安全的快照把最新测量值发布给 LVGL 界面；
         * 累计能耗由 mes_getEnergy() 在锁内读取，运行时间由 mes_getRunMs() 读取。
         * 能耗换算成 mJ 发布（界面按数值大小自动切换 mJ / J），
         * 超过 int32 量程时饱和，避免换算溢出为负数。 */
        const double energy_j = mes_getEnergy();
        int32_t energy_mj;
        if (energy_j <= 0.0) {
            energy_mj = 0;
        } else if (energy_j >= 2147483.0) {
            energy_mj = 2147483647;
        } else {
            energy_mj = (int32_t)(energy_j * 1000.0);
        }

        gui_publish_measurements((int32_t)v1, (int32_t)i1,
                                 (int32_t)w1, energy_mj, mes_getRunMs());

        if (++g_isrTime >= METER_RUNTIME_DIVIDER) {
            g_isrTime = 0;
            g_runTime++;
        }
    }
}

/* 启动采样：用 esp_timer 做 5 ms 时基，回调只通知采样任务。
 * FreeRTOS tick 默认 100 Hz（10 ms），因此周期 < 10 ms 时不用 vTaskDelay。 */
static void meter_init(void)
{
    BaseType_t created = xTaskCreate(meter_task, "meter", METER_TASK_STACK_SIZE,
                                     NULL, TASK_PRIO_METER, &s_meter_task);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    const esp_timer_create_args_t args = {
        .callback = meter_timer_cb,
        .arg = NULL,
        .name = "meter",
    };
    ESP_ERROR_CHECK(esp_timer_create(&args, &s_meter_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_meter_timer, METER_SAMPLE_PERIOD_MS * 1000));
}

/**********************
 *   APPLICATION MAIN
 **********************/
void app_main() {

    /* If you want to use a task to create the graphic, you NEED to create a Pinned task
     * Otherwise there can be problem such as memory corruption and so on.
     * NOTE: When not using Wi-Fi nor Bluetooth you can pin the guiTask to core 0 */
    ina226_init1();

//myusb_init();
//
mykeys_init();
//tempSensor_init();
 //xTaskCreate(usb_thread, "sockcli", 4096, NULL, 5, NULL);
 
 g_isrTimeP=0;
  g_isrTime=0;
  ina226_writeReg(1,5,2560);

 	//OLED_ShowString(0,0,(unsigned char *)"esp32 meter");
   //initI2CSlave();
   
   /* 启动采样：esp_timer 5 ms 周期通知采样任务 */
   meter_init();
   app_main_lvgl_drv();
   // xTaskCreatePinnedToCore(guiTask, "gui", 4096*8, NULL, 0, NULL, 1);
}
