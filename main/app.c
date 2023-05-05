#include "app.h"

/**********************
 *       WIDGETS
 **********************/

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_style_t style0_line_meter_1;
static lv_style_t style0_text_area_1;
static lv_style_t style0_line_meter_2;
static lv_style_t style0_text_area_2;
static lv_style_t style0_led_1;
static lv_style_t style0_line_meter_3;
static lv_style_t style0_image_1;

LV_IMG_DECLARE(default);


void app_create(lv_obj_t *parent)
{

#if LV_USE_LMETER
	lv_style_copy(&style0_line_meter_1, &lv_style_pretty_color);

	lv_obj_t *line_meter_1 = lv_lmeter_create(parent, NULL);
	lv_obj_set_pos(line_meter_1, 12, 17);
	lv_obj_set_size(line_meter_1, 100, 100);
	lv_lmeter_set_range(line_meter_1, 0, 0);
	lv_lmeter_set_value(line_meter_1, 0);
	lv_lmeter_set_angle_offset(line_meter_1, 0);
	lv_lmeter_set_scale(line_meter_1, 240, 240);
	lv_lmeter_set_style(line_meter_1, LV_LMETER_STYLE_MAIN, &style0_line_meter_1);
#endif // LV_USE_LMETER

#if LV_USE_TA
	lv_style_copy(&style0_text_area_1, &lv_style_pretty);

	lv_obj_t *text_area_1 = lv_ta_create(line_meter_1, NULL);
	lv_obj_set_pos(text_area_1, 12, 42);
	lv_obj_set_size(text_area_1, 78, 30);
	lv_ta_set_text(text_area_1, "Text area");
	lv_ta_set_placeholder_text(text_area_1, "");
	lv_ta_set_cursor_type(text_area_1, LV_CURSOR_LINE);
	lv_ta_set_cursor_blink_time(text_area_1, 400);
	lv_ta_set_one_line(text_area_1, false);
	lv_ta_set_pwd_mode(text_area_1, false);
	lv_ta_set_max_length(text_area_1, 0);
	lv_ta_set_scroll_propagation(text_area_1, false);
	lv_ta_set_edge_flash(text_area_1, false);
	lv_ta_set_style(text_area_1, LV_TA_STYLE_BG, &style0_text_area_1);
#endif // LV_USE_TA

#if LV_USE_LMETER
	lv_style_copy(&style0_line_meter_2, &lv_style_pretty_color);

	lv_obj_t *line_meter_2 = lv_lmeter_create(parent, NULL);
	lv_obj_set_pos(line_meter_2, 202, 22);
	lv_obj_set_size(line_meter_2, 100, 100);
	lv_lmeter_set_range(line_meter_2, 0, 0);
	lv_lmeter_set_value(line_meter_2, 0);
	lv_lmeter_set_angle_offset(line_meter_2, 0);
	lv_lmeter_set_scale(line_meter_2, 240, 240);
	lv_lmeter_set_style(line_meter_2, LV_LMETER_STYLE_MAIN, &style0_line_meter_2);
#endif // LV_USE_LMETER

#if LV_USE_TA
	lv_style_copy(&style0_text_area_2, &lv_style_pretty);

	lv_obj_t *text_area_2 = lv_ta_create(line_meter_2, NULL);
	lv_obj_set_pos(text_area_2, 10, 36);
	lv_obj_set_size(text_area_2, 81, 30);
	lv_ta_set_text(text_area_2, "Text area");
	lv_ta_set_placeholder_text(text_area_2, "");
	lv_ta_set_cursor_type(text_area_2, LV_CURSOR_LINE);
	lv_ta_set_cursor_blink_time(text_area_2, 400);
	lv_ta_set_one_line(text_area_2, false);
	lv_ta_set_pwd_mode(text_area_2, false);
	lv_ta_set_max_length(text_area_2, 0);
	lv_ta_set_scroll_propagation(text_area_2, false);
	lv_ta_set_edge_flash(text_area_2, false);
	lv_ta_set_style(text_area_2, LV_TA_STYLE_BG, &style0_text_area_2);
#endif // LV_USE_TA

#if LV_USE_LED
	lv_style_copy(&style0_led_1, &lv_style_pretty_color);

	lv_obj_t *led_1 = lv_led_create(parent, NULL);
	lv_obj_set_pos(led_1, 266, 166);
	lv_obj_set_size(led_1, 33, 33);
	lv_led_set_bright(led_1, 255);
	lv_led_set_style(led_1, LV_LED_STYLE_MAIN, &style0_led_1);
#endif // LV_USE_LED

#if LV_USE_LMETER
	lv_style_copy(&style0_line_meter_3, &lv_style_pretty_color);

	lv_obj_t *line_meter_3 = lv_lmeter_create(parent, NULL);
	lv_obj_set_pos(line_meter_3, 20, 128);
	lv_obj_set_size(line_meter_3, 100, 100);
	lv_lmeter_set_range(line_meter_3, 0, 0);
	lv_lmeter_set_value(line_meter_3, 0);
	lv_lmeter_set_angle_offset(line_meter_3, 0);
	lv_lmeter_set_scale(line_meter_3, 240, 240);
	lv_lmeter_set_style(line_meter_3, LV_LMETER_STYLE_MAIN, &style0_line_meter_3);
#endif // LV_USE_LMETER

#if LV_USE_IMG
	lv_style_copy(&style0_image_1, &lv_style_plain);

	lv_obj_t *image_1 = lv_img_create(parent, NULL);
	lv_obj_set_pos(image_1, 138, 106);
	lv_obj_set_size(image_1, 157, 53);
	lv_img_set_src(image_1, &default);
	lv_img_set_style(image_1, LV_IMG_STYLE_MAIN, &style0_image_1);
#endif // LV_USE_IMG

}
