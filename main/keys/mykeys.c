/*  按键、LED 指示与测量状态
 *
 *  KEY1/KEY2/KEY3（GPIO16/17/18）通过 GPIO 中断产生事件，由队列交给任务处理；
 *  LED 使用 LEDC 渐变（fade）实现呼吸效果，可在测量时启停。
 *
 *  测量状态（累计能量、峰值功率、运行标志）会被多个任务访问：
 *    - 采样任务：anaPower() 每个采样周期累加；
 *    - 按键任务 / 串口任务：startMes() / stopMes() 复位并启停；
 *    - 串口任务：mes_getEnergy() / mes_getPeak() 读取上报。
 *  因此这些状态统一用互斥锁 s_mes_lock 保护，不再以裸全局变量跨线程共享。
 */
#include "mykeys.h"

#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "ledfade.h"
#include "gui.h"
#include "task_prio.h"
#include "freertos/semphr.h"

static const char *TAG = "mykeys";

#define LEDC_TEST_DUTY         (4000)
#define LEDC_TEST_FADE_TIME    (2000)

#define GPIO_EVT_QUEUE_LEN     (10)
#define KEYS_TASK_STACK_SIZE   (2048)
#define LED_TASK_STACK_SIZE    (4096)

/* --- 跨线程共享的测量状态，全部由 s_mes_lock 保护 --- */
static SemaphoreHandle_t s_mes_lock = NULL;
static double  s_energy_j = 0.0;   /* 累计能量，单位 J */
static double  s_peak_mw  = 0.0;   /* 峰值功率，单位 mW */
static bool    s_running  = false; /* 是否正在测量 */
static int64_t s_start_us = 0;     /* 本次测量起始时刻（esp_timer 微秒） */

static TaskHandle_t ledTask;
static QueueHandle_t gpio_evt_queue = NULL;

static const ledc_channel_config_t ledc_channel = {
    .channel    = LEDC_LS_CH0_CHANNEL,
    .duty       = 0,
    .gpio_num   = LEDC_LS_CH0_GPIO,
    .speed_mode = LEDC_LS_MODE,
    .hpoint     = 0,
    .timer_sel  = LEDC_LS_TIMER,
    .intr_type  = LEDC_INTR_FADE_END,
};

static const ledc_timer_config_t ledc_timer = {
    .duty_resolution = LEDC_TIMER_13_BIT, // resolution of PWM duty
    .freq_hz = 5000,                      // frequency of PWM signal
    .speed_mode = LEDC_LS_MODE,           // timer mode
    .timer_num = LEDC_LS_TIMER,           // timer index
    .clk_cfg = LEDC_AUTO_CLK,             // Auto select the source clock
};

void startShow(void)
{
    if (ledTask == NULL) {
        return;
    }
    vTaskResume(ledTask);
    ledc_timer_resume(ledc_channel.speed_mode, LEDC_LS_TIMER);
}

void stopShow(void)
{
    if (ledTask == NULL) {
        return;
    }
    vTaskSuspend(ledTask);
    ledc_timer_pause(ledc_channel.speed_mode, LEDC_LS_TIMER);
}

void startMes(void)
{
    if (s_mes_lock == NULL) {
        return;
    }

    xSemaphoreTake(s_mes_lock, portMAX_DELAY);
    s_running = true;
    s_energy_j = 0.0;
    s_peak_mw = 0.0;
    s_start_us = esp_timer_get_time();
    startShow();
    xSemaphoreGive(s_mes_lock);

    gui_publish_running(true);
}

void stopMes(void)
{
    if (s_mes_lock == NULL) {
        return;
    }

    xSemaphoreTake(s_mes_lock, portMAX_DELAY);
    s_running = false;
    stopShow();
    xSemaphoreGive(s_mes_lock);

    gui_publish_running(false);
}

void anaPower(uint32_t elapsed_ms, double current_ma, double voltage_mv, double power_mw)
{
    if (s_mes_lock == NULL) {
        return;
    }

    xSemaphoreTake(s_mes_lock, portMAX_DELAY);

    if (s_running) {
        /* 峰值取瞬时功率，与积分时间无关，先更新 */
        if (power_mw > s_peak_mw) {
            s_peak_mw = power_mw;
        }

        if (elapsed_ms > 0) {
            /* i: mA, v: mV -> 瞬时功率 P(W) = i * v * 1e-3 * 1e-3
             * 本周期能量 E(J) = P(W) * elapsed_ms * 1e-3，累加到 s_energy_j */
            const double power_w = current_ma * voltage_mv * 0.001 * 0.001;
            s_energy_j += power_w * (elapsed_ms * 0.001);
        }
    }

    xSemaphoreGive(s_mes_lock);
}

double mes_getEnergy(void)
{
    if (s_mes_lock == NULL) {
        return 0.0;
    }

    xSemaphoreTake(s_mes_lock, portMAX_DELAY);
    const double value = s_energy_j;
    xSemaphoreGive(s_mes_lock);
    return value;
}

double mes_getPeak(void)
{
    if (s_mes_lock == NULL) {
        return 0.0;
    }

    xSemaphoreTake(s_mes_lock, portMAX_DELAY);
    const double value = s_peak_mw;
    xSemaphoreGive(s_mes_lock);
    return value;
}

int32_t mes_getRunMs(void)
{
    if (s_mes_lock == NULL) {
        return 0;
    }

    xSemaphoreTake(s_mes_lock, portMAX_DELAY);
    int32_t ms = 0;
    if (s_running) {
        const int64_t elapsed_us = esp_timer_get_time() - s_start_us;
        if (elapsed_us > 0) {
            ms = (int32_t)(elapsed_us / 1000);
        }
    }
    xSemaphoreGive(s_mes_lock);
    return ms;
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
    (void)arg;
    uint32_t io_num;

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            switch (io_num) {
            case MYKEY1:
                ESP_LOGI(TAG, "k1 down");
                startMes();
                break;
            case MYKEY2:
                ESP_LOGI(TAG, "k2 down");
                break;
            case MYKEY3:
                ESP_LOGI(TAG, "k3 down");
                stopMes();
                break;
            default:
                break;
            }
        }
    }
}

static void led_task_example(void *arg)
{
    (void)arg;

    while (1) {
        ledc_set_fade_with_time(ledc_channel.speed_mode,
                                ledc_channel.channel, LEDC_TEST_DUTY, LEDC_TEST_FADE_TIME);
        ledc_fade_start(ledc_channel.speed_mode,
                        ledc_channel.channel, LEDC_FADE_NO_WAIT);
        vTaskDelay(pdMS_TO_TICKS(LEDC_TEST_FADE_TIME));

        ledc_set_fade_with_time(ledc_channel.speed_mode,
                                ledc_channel.channel, 0, LEDC_TEST_FADE_TIME);
        ledc_fade_start(ledc_channel.speed_mode,
                        ledc_channel.channel, LEDC_FADE_NO_WAIT);
        vTaskDelay(pdMS_TO_TICKS(LEDC_TEST_FADE_TIME));
    }
}

void mykeys_init(void)
{
    /* 先建互斥锁，后面的任务才会去访问受保护的测量状态 */
    s_mes_lock = xSemaphoreCreateMutex();
    if (s_mes_lock == NULL) {
        ESP_LOGE(TAG, "create meter mutex failed");
        return;
    }

    const gpio_config_t io_conf = {
        .pin_bit_mask = MYKEYS_SEL,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(GPIO_EVT_QUEUE_LEN, sizeof(uint32_t));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "create gpio event queue failed");
        return;
    }

    //install gpio isr service
    esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "gpio isr service not installed: %s", esp_err_to_name(err));
    }

    ledFade_init();
    ESP_LOGI(TAG, "fade done");

    ESP_ERROR_CHECK(gpio_isr_handler_add(MYKEY1, gpio_isr_handler, (void *)MYKEY1));
    ESP_ERROR_CHECK(gpio_isr_handler_add(MYKEY2, gpio_isr_handler, (void *)MYKEY2));
    ESP_ERROR_CHECK(gpio_isr_handler_add(MYKEY3, gpio_isr_handler, (void *)MYKEY3));

    xTaskCreate(gpio_task_example, "gpio_task_example", KEYS_TASK_STACK_SIZE, NULL, TASK_PRIO_KEYS, NULL);
}

void ledFade_init(void)
{
    /*
     * Prepare and set configuration of timers
     * that will be used by LED Controller
     */
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    ESP_ERROR_CHECK(ledc_fade_func_install(0));

    ledc_set_fade_with_time(ledc_channel.speed_mode,
                            ledc_channel.channel, LEDC_TEST_DUTY, LEDC_TEST_FADE_TIME);
    ledc_fade_start(ledc_channel.speed_mode,
                    ledc_channel.channel, LEDC_FADE_NO_WAIT);

    /* led_task_example 创建后先挂起，由 startShow() 在测量开始时恢复 */
    xTaskCreate(led_task_example, "led_task_example", LED_TASK_STACK_SIZE, NULL, TASK_PRIO_LED, &ledTask);
    stopShow();
}
