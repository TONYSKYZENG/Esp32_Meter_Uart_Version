/*  UART0 命令收发
 *
 *  通过 UART0（引脚重映射到 GPIO7/GPIO8）接收上位机命令并返回测量数据。
 */
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "mykeys.h"
#include "uart0.h"

static const char *TAG = "uart0";

#define ECHO_TEST_TXD (GPIO_NUM_7)
#define ECHO_TEST_RXD (GPIO_NUM_8)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (UART_NUM_0)
#define ECHO_UART_BAUD_RATE     (1152000)
#define ECHO_TASK_STACK_SIZE    (2048)

#define BUF_SIZE                (1024)
#define ECHO_READ_TIMEOUT_MS    (20)

enum {
    UART_VCMD_START = 1,
    UART_VCMD_STOP,
    UART_VCMD_I,
    UART_VCMD_V,
    UART_VCMD_P,
    UART_VCMD_E,
    UART_VCMD_PEAK,
};

extern double g_power;
extern double g_peak;

/* 以原始字节形式回发一个 double 值 */
static void uart_send_double(double value)
{
    uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)&value, sizeof(value));
}

/* 解析收到的命令字节 */
static void parse_cmd(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        switch (data[i]) {
        case UART_VCMD_START:
            startMes();
            return;
        case UART_VCMD_STOP:
            stopMes();
            break;
        case UART_VCMD_PEAK:
            uart_send_double(g_peak);
            return;
        case UART_VCMD_E:
            uart_send_double(g_power);
            return;
        default:
            break;
        }
    }
}

static void echo_task(void *arg)
{
    (void)arg;

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    const uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    int intr_alloc_flags = 0;
#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    /* Configure a temporary buffer for the incoming data */
    uint8_t *data = (uint8_t *)malloc(BUF_SIZE);
    if (data == NULL) {
        ESP_LOGE(TAG, "no memory for rx buffer");
        vTaskDelete(NULL);
        return;
    }

    while (1) {
        /* Read data from the UART */
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, BUF_SIZE, pdMS_TO_TICKS(ECHO_READ_TIMEOUT_MS));
        if (len > 0) {
            parse_cmd(data, (size_t)len);
        }
    }
}

void initUart0(void)
{
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
}
