#include "ina226.h"

#include "freertos/FreeRTOS.h"

static const char *TAG = "ina226";

/* I2C 单次传输的超时时间（ms）。
 * 注意：不能传 0 —— 0 表示“不等待”，会立即返回 ESP_ERR_TIMEOUT 并复位 I2C 状态机，
 * 读到的寄存器数据仍是未初始化的栈内容，导致电压/电流/功率全是乱码。 */
#define INA226_I2C_TIMEOUT_MS   (100)

static gpio_num_t sda1 = 2;
static gpio_num_t scl1 = 1;
static i2c_port_t port1 = I2C_NUM_1;
static uint32_t i2c_frequency = 400000;

void ina226_init1(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda1,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = scl1,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = i2c_frequency,
        // .clk_flags = 0,          /*!< Optional, you can use I2C_SCLK_SRC_FLAG_* flags to choose i2c source clock here. */
    };
    ESP_ERROR_CHECK(i2c_param_config(port1, &conf));
    ESP_ERROR_CHECK(i2c_driver_install(port1, conf.mode,
                                       I2C_MASTER_RX_BUF_DISABLE,
                                       I2C_MASTER_TX_BUF_DISABLE, 0));
}

uint16_t ina226_readReg(i2c_port_t port, uint8_t addr)
{
    /* 必须初始化为 0：一旦传输失败，返回值就是这两个字节 */
    uint8_t dataMsb = 0;
    uint8_t dataLsb = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SlaveAddress, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SlaveAddress + 1, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &dataMsb, ACK_VAL);
    i2c_master_read_byte(cmd, &dataLsb, NACK_VAL);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(INA226_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "read reg 0x%02X failed: %s", addr, esp_err_to_name(ret));
        return 0;
    }

    return ((uint16_t)dataMsb << 8) | dataLsb;
}

void ina226_writeReg(i2c_port_t port, uint8_t addr, uint16_t val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, SlaveAddress, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, addr, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, (val >> 8) & 0xff, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, val & 0xff, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(INA226_I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "write reg 0x%02X failed: %s", addr, esp_err_to_name(ret));
    }
}

float ina226_getV(i2c_port_t port)
{
    /* Bus Voltage: LSB = 1.25 mV */
    return ina226_readReg(port, 0x02) * 1.25f;
}

float ina226_getI(i2c_port_t port)
{
    /* Current: LSB 由校准寄存器 (0x05) 决定，此处按 0.2 mA/LSB 换算 */
    return ina226_readReg(port, 0x04) * 0.2f;
}

float ina226_getW(i2c_port_t port)
{
    /* Power: LSB = 25 * Current_LSB，此处按 5 mW/LSB 换算 */
    return ina226_readReg(port, 0x03) * 5.0f;
}
