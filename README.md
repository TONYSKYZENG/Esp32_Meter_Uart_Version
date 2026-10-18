# ESP32-S2 UART Meter

基于 ESP-IDF 5.0+ 的 ESP32-S2 电参量测量 Demo：通过 I2C 读取 **INA226** 得到电压、电流、功率，
用 **3 个按键**控制测量的启停，并通过 **UART0** 与上位机进行二进制命令交互，返回累计能量与峰值功率。

本仓库还包含一个基于 **GPTimer** 的周期定时器封装，用于以固定周期触发采样。

## 功能概览

- INA226 电参量采集（电压 / 电流 / 功率），I2C 主机模式
- 按键控制测量启停，GPIO 下降沿中断 + 队列投递到任务处理
- 指示 LED 呼吸效果（LEDC 渐变），测量时点亮
- 能量累计与峰值功率保持
- UART0 命令接口：启停测量、读取累计能量 / 峰值功率
- GPTimer 周期定时器模块（1 us 分辨率，可同时使用两个定时器）

## 硬件与引脚

| 功能 | 外设 | 引脚 / 参数 | 说明 |
| --- | --- | --- | --- |
| INA226 数据线 | I2C SDA | GPIO2 | I2C 端口 `I2C_NUM_1`，400 kHz |
| INA226 时钟线 | I2C SCL | GPIO1 | 从机地址 `0x80`（写入地址） |
| 启动按键 KEY1 | GPIO 中断 | GPIO16 | 下降沿中断，按下后开始测量 |
| 备用按键 KEY2 | GPIO 中断 | GPIO17 | 预留，当前仅打印日志 |
| 停止按键 KEY3 | GPIO 中断 | GPIO18 | 下降沿中断，按下后停止测量 |
| 指示 LED | LEDC | GPIO21 | 低功耗通道 0，5 kHz / 13 bit，呼吸渐变 |
| 串口 TX | UART0 | GPIO7 | 发送数据到上位机 |
| 串口 RX | UART0 | GPIO8 | 接收上位机命令 |

UART0 参数：波特率 **1152000**，8 位数据位，无校验，1 位停止位，无流控。

## UART 命令协议

上位机发送单字节命令，模块按字节解析：

| 命令值 | 名称 | 动作 |
| --- | --- | --- |
| `0x01` | START | 开始测量：累计能量与峰值清零，LED 呼吸启动 |
| `0x02` | STOP | 停止测量：LED 呼吸暂停 |
| `0x06` | E | 回传累计能量 `g_power`（原始 8 字节 `double`） |
| `0x07` | PEAK | 回传峰值功率 `g_peak`（原始 8 字节 `double`） |

> `0x03` / `0x04` / `0x05`（读取电流 / 电压 / 功率）已在命令枚举中预留，当前未处理。

回传的 `double` 以原始内存字节直接写出，不含任何文本格式，上位机需按相同的字节序解析。

## 目录结构

```
main/
├── main.c              # 程序入口 app_main，初始化各模块
├── ina226/             # INA226 I2C 驱动：读写寄存器、读取 V/I/W
├── keys/               # 按键中断、LEDC 呼吸指示、测量启停与能量累计
├── uart0/              # UART0 命令收发任务
├── mytimer/            # GPTimer 周期定时器封装
├── gui.c, app.c …      # 显示相关代码，暂不在本文档覆盖范围内
config.sh  build.sh  debug.sh   # menuconfig / 编译 / 串口监视脚本
```

## 各模块说明

- **ina226**：初始化 I2C 主机；`ina226_readReg` / `ina226_writeReg` 按寄存器读写；
  `ina226_getV` / `ina226_getI` / `ina226_getW` 分别读取电压（0x02）、功率（0x03）、电流（0x04）寄存器并换算。
- **keys**：GPIO16/17/18 上拉输入、下降沿中断，通过队列交给任务处理；
  `startMes` / `stopMes` 控制测量状态与 LED 指示；`anaPower` 按采样做能量累计与峰值更新。
- **uart0**：`echo_task` 安装 UART0 驱动（引脚重映射到 GPIO7/8）并循环读取，
  `parse_cmd` 解析命令字节并执行相应动作。
- **mytimer**：`initTimerx(timer_idx, ms)` 创建并启动周期定时器，到期在中断回调中调用
  `t0Isr` / `t1Isr`；`deinitTimerx` 停止并释放。

## 工作流程

1. 上电后 `app_main` 依次执行：初始化 INA226 I2C → 初始化按键与指示灯 →
   写入 INA226 校准寄存器（`reg 5 = 2560`）→ 启动 UART0 接收任务。
2. 按下 KEY1 或收到 `START` 命令 → 开始测量：能量与峰值清零，LED 呼吸点亮。
3. 采样循环按周期触发（1 ms 定时标志）读取 INA226，调用 `anaPower` 累加能量并刷新峰值，
   每 200 次采样刷新一次显示 / 输出。
4. 按下 KEY3 或收到 `STOP` 命令 → 停止测量，LED 呼吸暂停。
5. 上位机可随时发送 `E` / `PEAK` 命令读取当前累计能量或峰值功率。

## 编译与运行

依赖已配置好的 ESP-IDF 环境，目标芯片为 **esp32s2**。仓库内提供三个脚本：

```sh
./config.sh   # idf.py menuconfig
./build.sh    # idf.py build
./debug.sh    # idf.py monitor
```

脚本中 `IDF_PATH` 默认为 `~/Platforms/esp/esp-idf`，如与实际环境不同请自行修改。

## 当前状态

- 当前加入编译的源文件为：`main.c`、`ina226/ina226.c`、`keys/mykeys.c`、`uart0/uart0.c`、`mytimer/mytimer.c`。
- 周期采样与刷新循环位于 `main/gui.c`，目前未加入 `main/CMakeLists.txt` 的编译列表，
  `app_main` 中对应的任务创建也处于注释状态。因此当前固件完成的是**各模块初始化 + 按键 + UART 命令**，
  完整的采样/显示循环尚未启用。
- `myusb_init`、`tempSensor_init`、`initI2CSlave` 等调用在 `app_main` 中已注释，属于预留功能。
