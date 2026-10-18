/*  按键与 LED 指示
 *
 *  KEY1/KEY2/KEY3（GPIO16/17/18）通过 GPIO 中断产生事件，由队列交给任务处理；
 *  LED 使用 LEDC 渐变（fade）实现呼吸效果，可在测量时启停。
 */
#include "mykeys.h"

#include "esp_log.h"
#include "ledfade.h"

static const char *TAG = "mykeys";

#define LEDC_TEST_DUTY         (4000)
#define LEDC_TEST_FADE_TIME    (2000)

#define GPIO_EVT_QUEUE_LEN     (10)
#define KEYS_TASK_STACK_SIZE   (2048)
#define LED_TASK_STACK_SIZE    (4096)
#define KEYS_TASK_PRIO         (10)
#define LED_TASK_PRIO          (10)

extern double g_i0, g_v0, g_i1, g_v1, g_w1;

double g_power = 0;
double g_peak = 0;
uint8_t g_workSt = 0;

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
    g_workSt = 1;
    g_power = 0.0;
    startShow();
    g_peak = 0;
}

void stopMes(void)
{
    g_workSt = 0;
    stopShow();
}

void anaPower(void)
{
    double temp_i;

    if (g_workSt) {
        temp_i = g_i0 + g_i1;
        g_power += temp_i * g_v1 * 0.001 * 0.001 * 0.001;
        if (g_w1 > g_peak) {
            g_peak = g_w1;
        }
    }
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

    xTaskCreate(gpio_task_example, "gpio_task_example", KEYS_TASK_STACK_SIZE, NULL, KEYS_TASK_PRIO, NULL);
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
    xTaskCreate(led_task_example, "led_task_example", LED_TASK_STACK_SIZE, NULL, LED_TASK_PRIO, &ledTask);
    stopShow();
}
