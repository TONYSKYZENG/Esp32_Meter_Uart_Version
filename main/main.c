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
#include "ina226.h"
#include "mykeys.h"
#include "uart0.h"
uint16_t g_runTime=0;
uint16_t g_isrTime=0;
uint16_t g_isrTimeP=0;
uint8_t g_isTxData=0;
uint16_t g_ly68Id=0;
//double g_i0,g_v0,g_i1,g_v1,g_w1;
/* Littlevgl specific */


/*********************
 *      DEFINES
 *********************/
#define TAG "demo"
#define LV_TICK_PERIOD_MS 1

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void lv_tick_task(void *arg);
static void guiTask(void *pvParameter);
static void create_demo_application(void);

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
   initUart0();
   // xTaskCreatePinnedToCore(guiTask, "gui", 4096*8, NULL, 0, NULL, 1);
}