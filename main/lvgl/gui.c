/*  电表界面 (LVGL 9.2)
 *
 *  布局：
 *    顶部一行并排的四套电表组件，每套 = 一个 arc（圆环指针）+ 一个文本显示区（label）；
 *    底部为状态指示用的 lv_led。
 *
 *  四套表：电压、电流、功率、总能耗（焦耳）。
 *  电流单位为自适应：不足 1 A 时显示 mA，否则显示 A。
 *
 *  线程安全：
 *    LVGL 不是线程安全的，所有 widget 操作都只发生在 LVGL 任务里（本文件里
 *    lvgl_demo_ui() 用于建界面、gui_refresh_cb() 用于刷新，两者都在 LVGL 任务
 *    上下文执行）。采样任务和按键任务不直接读测量全局量，而是通过
 *    gui_publish_measurements() / gui_publish_running() 把数据写入一份由
 *    临界区（spinlock）保护的快照；刷新定时器再取快照更新界面。
 */

#include "gui.h"

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "gui";

/*********************
 *      配置
 *********************/
#define METER_COUNT         (4)      /* 电表套数 */
#define METER_ARC_SIZE      (64)     /* arc 直径 */
#define METER_CELL_W        (78)     /* 每套电表区域宽度 */
#define METER_CELL_H        (150)    /* 每套电表区域高度 */
#define METER_ROW_Y         (8)      /* 表盘行距屏幕顶部的距离 */

#define GUI_REFRESH_PERIOD_MS       (200) /* 电表刷新周期 */
#define GUI_TIME_REFRESH_PERIOD_MS  (50)  /* 运行时间文本刷新周期 */

#define METER_MILLI_DIVIDER    (1000) /* mV/mA/mW -> V/A/W 的换算除数 */

/**********************
 *      类型定义
 **********************/
/* 数值显示方式 */
typedef enum {
    METER_FMT_MILLI,    /* 毫单位 -> 基本单位，两位小数（电压、功率） */
    METER_FMT_CURR,     /* 电流：自适应 mA / A */
    METER_FMT_ENERGY,   /* 能耗：自适应 mJ / J */
} meter_fmt_t;

typedef struct {
    const char *name;   /* 文本显示区第一行：表名 */
    const char *unit;   /* 默认单位（电流 / 能耗自适应时会被动态覆盖） */
    int32_t     max;    /* arc 满量程（原始单位） */
    uint32_t    color;  /* arc 指示条颜色 */
    meter_fmt_t fmt;    /* 数值显示方式 */
} meter_cfg_t;

typedef struct {
    lv_obj_t *arc;      /* 圆环 */
    lv_obj_t *label;    /* 文本显示区 */
} meter_ui_t;

/**********************
 *  静态数据 / 对象
 **********************/
/* 四套表：电压 / 电流 / 功率 / 总能耗 */
static const meter_cfg_t s_meter_cfg[METER_COUNT] = {
    { "VOLT", "V", 30000,  0x2ECC71, METER_FMT_MILLI  },  /* 0 ~ 30 V   */
    { "CURR", "",  5000,   0x3498DB, METER_FMT_CURR   },  /* 0 ~ 5 A    */
    { "POWR", "W", 100000, 0xE67E22, METER_FMT_MILLI  },  /* 0 ~ 100 W  */
    { "ENRG", "J", 100000000, 0x9B59B6, METER_FMT_ENERGY },  /* 0 ~ 100 kJ（原始单位 mJ） */
};

static meter_ui_t s_meter_ui[METER_COUNT];

static lv_obj_t *s_led;         /* 状态 LED */
static lv_obj_t *s_led_label;   /* LED 旁的状态文字 */
static lv_obj_t *s_time_label;  /* 测量运行时间文字 */

lv_obj_t *window_home;          /* 根屏幕对象 */

/**********************
 *  跨线程共享的显示数据
 **********************
 * 由生产者（采样任务 / 按键任务）写入，由 LVGL 任务读取，
 * 用临界区保护，保证整份快照的一致性读写。
 */
typedef struct {
    int32_t voltage_mv;
    int32_t current_ma;
    int32_t power_mw;
    int32_t energy_mj;
    int32_t elapsed_ms;
    bool    running;
} gui_data_t;

static gui_data_t s_data;
static portMUX_TYPE s_data_mux = portMUX_INITIALIZER_UNLOCKED;

void gui_publish_measurements(int32_t voltage_mv, int32_t current_ma,
                              int32_t power_mw, int32_t energy_mj,
                              int32_t elapsed_ms)
{
    portENTER_CRITICAL(&s_data_mux);
    s_data.voltage_mv = voltage_mv;
    s_data.current_ma = current_ma;
    s_data.power_mw   = power_mw;
    s_data.energy_mj  = energy_mj;
    s_data.elapsed_ms = elapsed_ms;
    portEXIT_CRITICAL(&s_data_mux);
}

void gui_publish_running(bool running)
{
    portENTER_CRITICAL(&s_data_mux);
    s_data.running = running;
    portEXIT_CRITICAL(&s_data_mux);
}

/* 只在 LVGL 任务里调用：取一份完整快照 */
static void gui_data_get(gui_data_t *out)
{
    portENTER_CRITICAL(&s_data_mux);
    *out = s_data;
    portEXIT_CRITICAL(&s_data_mux);
}

/**********************
 *   内部函数
 **********************/

/* 按配置把原始读数格式化成字符串，并通过 *unit 返回实际使用的单位 */
static void meter_format(const meter_cfg_t *cfg, int32_t raw,
                         char *buf, size_t buf_size, const char **unit)
{
    const char *sign = (raw < 0) ? "-" : "";
    const int32_t a = (raw < 0) ? -raw : raw;

    *unit = cfg->unit;

    switch (cfg->fmt) {
    case METER_FMT_CURR:
        if (a < METER_MILLI_DIVIDER) {
            /* 不足 1 A：直接用 mA，整数 */
            *unit = "mA";
            lv_snprintf(buf, buf_size, "%s%d", sign, (int)a);
        } else {
            /* 1 A 及以上：换算成 A，两位小数 */
            *unit = "A";
            lv_snprintf(buf, buf_size, "%s%d.%02d",
                        sign, (int)(a / METER_MILLI_DIVIDER),
                        (int)((a % METER_MILLI_DIVIDER) / (METER_MILLI_DIVIDER / 100)));
        }
        break;

    case METER_FMT_ENERGY:
        if (a < METER_MILLI_DIVIDER) {
            /* 不足 1 J：直接用 mJ，整数 */
            *unit = "mJ";
            lv_snprintf(buf, buf_size, "%s%d", sign, (int)a);
        } else {
            /* 1 J 及以上：换算成 J，两位小数 */
            *unit = "J";
            lv_snprintf(buf, buf_size, "%s%d.%02d",
                        sign, (int)(a / METER_MILLI_DIVIDER),
                        (int)((a % METER_MILLI_DIVIDER) / (METER_MILLI_DIVIDER / 100)));
        }
        break;

    case METER_FMT_MILLI:
    default:
        /* 毫单位 -> 基本单位，两位小数 */
        lv_snprintf(buf, buf_size, "%s%d.%02d",
                    sign, (int)(a / METER_MILLI_DIVIDER),
                    (int)((a % METER_MILLI_DIVIDER) / (METER_MILLI_DIVIDER / 100)));
        break;
    }
}

/* 创建单套电表组件（arc + 文本显示区），挂到 row 容器上 */
static void meter_create(lv_obj_t *parent, uint8_t idx)
{
    const meter_cfg_t *cfg = &s_meter_cfg[idx];
    meter_ui_t *ui = &s_meter_ui[idx];

    /* 每套电表的容器 */
    lv_obj_t *cell = lv_obj_create(parent);
    lv_obj_set_size(cell, METER_CELL_W, METER_CELL_H);
    lv_obj_set_style_bg_opa(cell, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(cell, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);
    lv_obj_remove_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    /* --- arc：圆环 --- */
    ui->arc = lv_arc_create(cell);
    lv_obj_set_size(ui->arc, METER_ARC_SIZE, METER_ARC_SIZE);
    lv_obj_align(ui->arc, LV_ALIGN_TOP_MID, 0, 6);
    lv_obj_remove_flag(ui->arc, LV_OBJ_FLAG_CLICKABLE);  /* 只显示，不可拖动 */

    lv_arc_set_rotation(ui->arc, 135);                   /* 缺口朝下 */
    lv_arc_set_bg_angles(ui->arc, 0, 270);
    lv_arc_set_range(ui->arc, 0, cfg->max);
    lv_arc_set_value(ui->arc, 0);

    /* 轨道与指示条外观 */
    lv_obj_set_style_arc_width(ui->arc, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ui->arc, 9, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ui->arc, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(ui->arc, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ui->arc, lv_color_hex(0x303030), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ui->arc, lv_color_hex(cfg->color), LV_PART_INDICATOR);
    lv_obj_remove_style(ui->arc, NULL, LV_PART_KNOB);    /* 隐藏旋钮 */

    /* --- label：文本显示区 --- */
    ui->label = lv_label_create(cell);
    lv_obj_set_width(ui->label, METER_CELL_W - 2);
    lv_obj_align(ui->label, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_obj_set_style_text_align(ui->label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    const char *unit;
    char value_str[20];
    meter_format(cfg, 0, value_str, sizeof(value_str), &unit);
    lv_label_set_text_fmt(ui->label, "%s\n%s %s", cfg->name, value_str, unit);
}

/* 把第 idx 套电表的数值刷到界面上（仅 LVGL 任务调用） */
static void meter_apply(uint8_t idx, int32_t raw)
{
    if (idx >= METER_COUNT) {
        return;
    }

    const meter_cfg_t *cfg = &s_meter_cfg[idx];
    meter_ui_t *ui = &s_meter_ui[idx];
    if (ui->arc == NULL || ui->label == NULL) {
        return;
    }

    /* arc：把读数按量程裁剪后显示 */
    int32_t arc_val = raw;
    if (arc_val < 0) {
        arc_val = 0;
    } else if (arc_val > cfg->max) {
        arc_val = cfg->max;
    }
    lv_arc_set_value(ui->arc, arc_val);

    /* 文本区：名称 + 换算后的数值 + 单位 */
    const char *unit;
    char value_str[20];
    meter_format(cfg, raw, value_str, sizeof(value_str), &unit);
    lv_label_set_text_fmt(ui->label, "%s\n%s %s", cfg->name, value_str, unit);
}

/* 刷新状态 LED（仅 LVGL 任务调用） */
static void led_apply(bool on)
{
    if (s_led == NULL) {
        return;
    }

    if (on) {
        lv_led_on(s_led);
    } else {
        lv_led_off(s_led);
    }
    if (s_led_label != NULL) {
        lv_label_set_text(s_led_label, on ? "RUN" : "IDLE");
    }
}

/* 周期性刷新（在 LVGL 任务里执行）：取快照 -> 更新四套表和 LED */
static void gui_refresh_cb(lv_timer_t *timer)
{
    (void)timer;

    gui_data_t d;
    gui_data_get(&d);

    meter_apply(0, d.voltage_mv);
    meter_apply(1, d.current_ma);
    meter_apply(2, d.power_mw);
    meter_apply(3, d.energy_mj);
    led_apply(d.running);
}

/* 运行时间刷新（在 LVGL 任务里执行）：测量中显示已运行秒数（三位小数），停止时清空 */
static void gui_time_refresh_cb(lv_timer_t *timer)
{
    (void)timer;

    if (s_time_label == NULL) {
        return;
    }

    gui_data_t d;
    gui_data_get(&d);

    if (d.running) {
        lv_label_set_text_fmt(s_time_label, "%d.%03d s",
                              (int)(d.elapsed_ms / 1000),
                              (int)(d.elapsed_ms % 1000));
    } else {
        lv_label_set_text(s_time_label, "");
    }
}

/**********************
 *   界面入口
 **********************/
void lvgl_demo_ui(lv_disp_t *disp)
{
    window_home = lv_display_get_screen_active(disp);
    lv_disp_set_rotation(disp, LV_DISP_ROTATION_270);

    /* 屏幕本身不需要滚动 */
    lv_obj_remove_flag(window_home, LV_OBJ_FLAG_SCROLLABLE);

    /* 顶部一行：并排放置四套电表 */
    lv_obj_t *row = lv_obj_create(window_home);
    lv_obj_set_size(row, LV_PCT(100), METER_CELL_H);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, METER_ROW_Y);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    for (uint8_t i = 0; i < METER_COUNT; i++) {
        meter_create(row, i);
    }

    /* 底部：状态 LED 及其文字 */
    s_led = lv_led_create(window_home);
    lv_obj_set_size(s_led, 18, 18);
    lv_led_set_color(s_led, lv_color_hex(0x2ECC71));
    lv_led_set_brightness(s_led, 255);
    lv_led_off(s_led);
    lv_obj_align(s_led, LV_ALIGN_BOTTOM_MID, -46, -22);

    s_led_label = lv_label_create(window_home);
    lv_label_set_text(s_led_label, "IDLE");
    lv_obj_align(s_led_label, LV_ALIGN_BOTTOM_MID, 8, -24);

    /* 底部右侧：测量运行时间（测量中显示秒数，三位小数；停止时清空） */
    s_time_label = lv_label_create(window_home);
    lv_label_set_text(s_time_label, "");
    lv_obj_align(s_time_label, LV_ALIGN_BOTTOM_RIGHT, -10, -24);

    /* 周期性把快照刷到界面上（回调运行在 LVGL 任务） */
    lv_timer_create(gui_refresh_cb, GUI_REFRESH_PERIOD_MS, NULL);
    lv_timer_create(gui_time_refresh_cb, GUI_TIME_REFRESH_PERIOD_MS, NULL);

    ESP_LOGI(TAG, "meter UI created: %d meters + 1 led + run time", METER_COUNT);
}
