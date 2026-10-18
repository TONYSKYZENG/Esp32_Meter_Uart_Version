# ESP32-S2 UART Meter

基于 ESP-IDF 5.0+ 的 ESP32-S2 电参量测量 Demo：通过 I2C 读取 **INA226** 得到电压、电流、功率，
用 **3 个按键**控制测量的启停，并通过 **UART0** 与上位机进行二进制命令交互，返回累计能量与峰值功率；
同时在板载 **SPI 液晶（ST7789）**上用 **LVGL 9.2** 绘制电表界面。

本仓库还包含一个基于 **GPTimer** 的周期定时器封装，用于以固定周期触发采样。

## 功能概览

- INA226 电参量采集（电压 / 电流 / 功率），I2C 主机模式
- 按键控制测量启停，GPIO 下降沿中断 + 队列投递到任务处理
- 指示 LED 呼吸效果（LEDC 渐变），测量时点亮
- 能量累计与峰值功率保持
- UART0 命令接口：启停测量、读取累计能量 / 峰值功率
- 板载 SPI 液晶界面（LVGL 9.2）：四套电表（电压 / 电流 / 功率 / 累计能耗），
  每套 = 圆环 arc + 数值文本；底部运行状态 LED 与测量运行时间（秒，三位小数）
- 能耗 / 电流按数值大小自动切换单位（mJ/J、mA/A）
- 界面与采样解耦：采样任务只发布线程安全快照，LVGL 任务独立定时刷新
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
| LCD SPI 时钟 | SPI SCLK | GPIO41 | `SPI2_HOST`，20 MHz |
| LCD SPI 数据 | SPI MOSI | GPIO40 | LCD 的 SDA |
| LCD 数据/命令 | LCD DC | GPIO38 | |
| LCD 复位 | LCD RST | GPIO39 | |
| LCD 片选 | LCD CS | GPIO42 | |
| LCD 背光 | BK_LIGHT | -1 | 未使用（由硬件常亮） |

UART0 参数：波特率 **115200**，8 位数据位，无校验，1 位停止位，无流控。
样例原理图可参见 **sch_meter.pdf**

## UART 命令协议

上位机发送单字节命令，模块按字节解析：

| 命令值 | 名称 | 动作 |
| --- | --- | --- |
| `0x01` | START | 开始测量：累计能量与峰值清零，LED 呼吸启动 |
| `0x02` | STOP | 停止测量：LED 呼吸暂停 |
| `0x06` | E | 回传累计能量 `mes_getEnergy()`（原始 8 字节 `double`） |
| `0x07` | PEAK | 回传峰值功率 `mes_getPeak()`（原始 8 字节 `double`） |

> `0x03` / `0x04` / `0x05`（读取电流 / 电压 / 功率）已在命令枚举中预留，当前未处理。

回传的 `double` 以原始内存字节直接写出，不含任何文本格式，上位机需按相同的字节序解析。

上位机可以使用 test.py 作为最小测试。

## 目录结构

```
main/
├── main.c              # 程序入口 app_main，初始化各模块
├── ina226/             # INA226 I2C 驱动：读写寄存器、读取 V/I/W
├── keys/               # 按键中断、LEDC 呼吸指示、测量启停与能量累计
├── uart0/              # UART0 命令收发任务
├── mytimer/            # GPTimer 周期定时器封装
├── lvgl/
│   ├── boot_strap.c    # 显示移植层：SPI LCD 驱动、LVGL 任务与 tick
│   ├── gui.c           # 电表界面：四套 arc + 文本 + 状态 LED
│   └── gui.h           # 显示数据发布接口（线程安全）
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
- **lvgl/boot_strap.c**：显示移植层。初始化 SPI2 总线与 ST7789 面板（240×320，SPI 20 MHz），
  创建 LVGL 显示与双缓冲，注册 tick 定时器与 flush 回调，随后在 LVGL 任务里调用
  `lvgl_demo_ui()` 建界面；任务固定到 CPU0，最后调用 `initUart0()` 启动串口。
- **lvgl/gui.c / gui.h**：电表界面。`lvgl_demo_ui()` 在屏幕顶部并排创建四套电表
  （电压 / 电流 / 功率 / 累计能耗），每套由圆环 `lv_arc` 与数值文本组成，底部一个状态 LED 和
  一个测量运行时间文本；`gui_refresh_cb`（200 ms）刷新电表，`gui_time_refresh_cb`（50 ms）
  刷新运行时间。对外只暴露 `gui_publish_measurements()` 与 `gui_publish_running()`，由采样任务 /
  按键任务写入受临界区保护的快照，避免跨线程操作 LVGL。

## 显示界面（GUI）

- 屏幕：**ST7789** SPI 液晶，240×320，LVGL 中旋转 270°，实际按横屏 320×240 布局。
- 布局：顶部一行并排 **四套电表**，底部居中一个运行状态 LED，底部右侧显示测量运行时间。
- 四套电表（`main/lvgl/gui.c` 中的 `s_meter_cfg`）：

  | 名称 | 含义 | 量程 | arc 颜色 | 显示格式 |
  | --- | --- | --- | --- | --- |
  | `VOLT` | 电压 | 0 ~ 30 V | 绿 | 两位小数 |
  | `CURR` | 电流 | 0 ~ 5 A | 蓝 | 自适应：<1 A 显示 mA，否则 A |
  | `POWR` | 功率 | 0 ~ 100 W | 橙 | 两位小数 |
  | `ENRG` | 累计能耗 | 0 ~ 100 kJ | 紫 | 自适应：<1 J 显示 mJ，否则 J（两位小数） |

- 状态 LED：测量中显示绿色并置 `RUN`，停止时为 `IDLE`。
- 运行时间：测量中在底部右侧显示已运行秒数，格式如 `12.345 s`（三位小数）；停止时清空。
- 刷新周期：电表 **200 ms**（`GUI_REFRESH_PERIOD_MS`），运行时间 **50 ms**
  （`GUI_TIME_REFRESH_PERIOD_MS`）。数值超出 arc 量程时只裁剪圆环指示，文本仍显示真实值。

## 工作流程

1. 上电后 `app_main` 依次执行：初始化 INA226 I2C → 初始化按键与指示灯 →
   写入 INA226 校准寄存器（`reg 5 = 2560`）→ 启动采样定时器与任务（`meter_init`）→
   初始化 SPI 液晶与 LVGL 并创建界面（`app_main_lvgl_drv`，其中会调用 `initUart0` 启动串口接收任务）。
2. 按下 KEY1 或收到 `START` 命令 → 开始测量：能量与峰值清零，LED 呼吸点亮，
   同时通过 `gui_publish_running(true)` 让界面状态 LED 显示 RUN。
3. 采样循环由 `esp_timer` 以 **5 ms** 周期通知采样任务触发，读取 INA226，调用 `anaPower` 按实际
   经过时间累加能量并刷新峰值，每 200 次采样累计一次运行时间；随后用 `gui_publish_measurements()`
   把最新测量值与累计能耗发布给界面。
4. LVGL 任务中的 `gui_refresh_cb` 每 200 ms 取一次快照，更新四套电表与状态 LED；
   同时 `gui_time_refresh_cb` 每 50 ms 把已运行时间刷新为秒（三位小数），停止时清空。
5. 按下 KEY3 或收到 `STOP` 命令 → 停止测量，LED 呼吸暂停，界面状态 LED 复位为 IDLE。
6. 上位机可随时发送 `E` / `PEAK` 命令读取当前累计能量或峰值功率。

## 编译与运行

依赖已配置好的 ESP-IDF 环境，目标芯片为 **esp32s2**。仓库内提供三个脚本：

```sh
./config.sh   # idf.py menuconfig
./build.sh    # idf.py build
./debug.sh    # idf.py monitor
```

脚本中 `IDF_PATH` 默认为 `~/Platforms/esp/esp-idf`，如与实际环境不同请自行修改。

组件依赖由组件管理器自动拉取（见 `main/idf_component.yml`）：`lvgl/lvgl` 9.2.0、
`esp_lcd_ili9341`、`esp_lcd_gc9a01`。当前实际使用的是 ESP-IDF 自带的 ST7789 面板驱动，
显示引脚与分辨率在 `main/lvgl/boot_strap.c` 顶部宏定义，可根据实际屏幕修改。

## 当前状态

- 当前加入编译的源文件为：`main.c`、`ina226/ina226.c`、`keys/mykeys.c`、`uart0/uart0.c`、
  `mytimer/mytimer.c`、`lvgl/gui.c`、`lvgl/boot_strap.c`。
- 采样/能量累计循环位于 `main.c`：`meter_init()` 通过 `esp_timer` 注册一个 **5 ms** 周期定时器
  （回调 `meter_timer_cb` 只做任务通知），由采样任务 `meter_task` 读取 INA226 并调用 `anaPower`，
  每 200 次采样累计一次运行时间；不再经过 `mytimer` / GPTimer 封装（`mytimer` 仍在编译列表中，
  但采样已不使用它）。
- 显示界面已加入编译：`lvgl/boot_strap.c` 负责 LCD/LVGL 移植与任务，`lvgl/gui.c` 负责电表界面；
  `app_main` 末尾调用 `app_main_lvgl_drv()` 启动显示，界面数据通过线程安全快照从采样任务获取。
- 因此当前固件完成的是**各模块初始化 + 5 ms 采样/能量累计 + 按键 + UART 命令 + LVGL 电表界面**。
- 旧的根目录显示代码（`gui.c` / `app.c`）已被 `main/lvgl/` 下的实现取代，当前不在编译列表中。
- `myusb_init`、`tempSensor_init`、`initI2CSlave` 等调用在 `app_main` 中已注释，属于预留功能。
