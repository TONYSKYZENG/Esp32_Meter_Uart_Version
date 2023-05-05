#include <stdio.h>
#include <string.h>
#include "soc/uart_periph.h"
#include "driver/uart.h"
#include "soc/uhci_reg.h"
#include "driver/gpio.h"
#include "esp_event.h"
#include "esp_log.h"
#include "mykeys.h"
enum {
    UART_VCMD_START=1,
    UART_VCMD_STOP,
    UART_VCMD_I,
    UART_VCMD_V,
    UART_VCMD_P,
    UART_VCMD_E,
   UART_VCMD_PEAK,

};
extern double g_i0,g_v0,g_i1,g_v1,g_w1;
extern double g_power;
extern double g_peak;
#define ECHO_TEST_TXD (GPIO_NUM_7)
#define ECHO_TEST_RXD (GPIO_NUM_8)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (UART_NUM_0)
#define ECHO_UART_BAUD_RATE     (1152000)
#define ECHO_TASK_STACK_SIZE    (2048)

#define BUF_SIZE (1024)
uint8_t paraseCmd(uint8_t *data,size_t len)
{uint8_t *str=(uint8_t*)"hello world";
     for(size_t i=0;i<len;i++)
        {
            switch (data[i])
            {
              case UART_VCMD_START:
                  startMes();
              //  printf("start meas\r\n");
                 return 0;
            case UART_VCMD_STOP:
                stopMes();
                 // printf("stop meas\r\n");
                break ;
            case UART_VCMD_PEAK:
             uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) &g_peak, sizeof(double));
               // uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)"hello world", sizeof(double));
                
                return;
            case UART_VCMD_E:
           // txData=(uint8_t *)(&g_power);
          // printf("%lf\r\n",g_power);
             uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) &g_power, sizeof(double));
               // uart_write_bytes(ECHO_UART_PORT_NUM, (const char *)"hello world", sizeof(double));
                
                return;
            }
        }
}
static void echo_task(void *arg)
{
    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);

    while (1) {
        // Read data from the UART
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, BUF_SIZE, 20 / portTICK_RATE_MS);
        // Write data back to the UART
      //  uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) data, len);
      uint64_t *txData;
    paraseCmd(data,len);
    }
}

void initUart0(void)
{
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
}