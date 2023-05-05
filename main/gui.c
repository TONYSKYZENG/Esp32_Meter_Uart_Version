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

/* Littlevgl specific */
#ifdef LV_LVGL_H_INCLUDE_SIMPLE
#include "lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#include "lvgl_helpers.h"

#include "ina226.h"
#include "mykeys.h"
#include "uart0.h"
#include "mytimer.h"
lv_obj_t *line_meter_1, **line_meter_2,*line_meter_3;
lv_obj_t *text_area_1,*text_area_2,*text_area_3,*text_area_4;
extern uint16_t g_runTime;
extern uint16_t g_isrTime;
double g_i0,g_v0,g_i1,g_v1,g_w1;
uint16_t g_isrTimeP;
extern uint8_t g_workSt;
 extern double g_power;
 lv_obj_t *led_1;
 void t0Isr(void)
{
  //while (g_isrTimeP==1);
 g_isrTimeP=1;
 
}

void t1Isr(void)
{
  //sys_display();
}
void guiMaintain(void *pvParameter) {
    char str[128];
  initTimerx(TIMER_0,1.0);
  while (1)
  {
    if(g_isrTimeP==0)
  {
    continue;
  }
	g_isrTime++;
   g_v1=ina226_getV(1);
   g_i1=ina226_getI(1);
   g_w1 = ina226_getW(1);
   anaPower();
        if(g_isrTime==200)
        {  
          // temp_sensor_read_celsius(&tsens_out);
              g_runTime++;
              g_isrTime=0;


            
             sprintf(str,"%4.0f mV",g_v1);	
             lv_linemeter_set_value(line_meter_1, g_v1);
             lv_textarea_set_text(text_area_1, str);
   		//OLED_ShowString(0,30,(unsigned char *)str);
      sprintf(str,"%4.0f mA",g_i1);	
        lv_textarea_set_text(text_area_2, str);
         lv_linemeter_set_value(line_meter_2, g_i1);
   		//OLED_ShowString(0,20,(unsigned char *)str);
        sprintf(str,"%4.0f mW",g_w1);	
         lv_linemeter_set_value(line_meter_3, g_w1);
            lv_textarea_set_text(text_area_3, str);
              if(g_workSt)
            {
                 sprintf(str,"%6.2lf J,run",g_power);	
            }
            else
            {
               sprintf(str,"%6.2lf J,stop",g_power);	
            }
           
        lv_textarea_set_text(text_area_4, str);
          // temp_sensor_read_celsius(&tsens_out);
        //	OLED_ShowString(0,10,(unsigned char *)str);
         /*  OLED_ShowString(0,50,(unsigned char *)str);
       sprintf(str,"U=%6.2f mV",g_v1);	
   		OLED_ShowString(0,30,(unsigned char *)str);
      sprintf(str,"I=%6.2f mA",g_i1);	
   		OLED_ShowString(0,20,(unsigned char *)str);
        sprintf(str,"P=%6.2f mW",g_w1);	
   		OLED_ShowString(0,10,(unsigned char *)str);*/
  
            //sys_display();// 
        }
       g_isrTimeP=0;

  }
  


}

LV_IMG_DECLARE(logo);
void app_create(lv_obj_t *parent)
{


	//lv_style_copy(&style0_line_meter_1, &lv_style_pretty_color);

	line_meter_1 = lv_linemeter_create(parent, NULL);
	lv_obj_set_pos(line_meter_1, 0, 0);
	lv_obj_set_size(line_meter_1, 100, 100);
	lv_linemeter_set_range(line_meter_1, 4500, 5500);
	lv_linemeter_set_value(line_meter_1, 5000);
	//lv_linemeter_set_scale(line_meter_1, 240, 240);
    text_area_1 = lv_textarea_create(parent, NULL);
	lv_obj_set_pos(text_area_1, 0, 80);
	lv_obj_set_size(text_area_1, 100, 26);
	lv_textarea_set_text(text_area_1, "5000 mV");
    lv_textarea_set_cursor_hidden(text_area_1,true);


    line_meter_2 = lv_linemeter_create(parent, NULL);
	lv_obj_set_pos(line_meter_2, 200, 0);
	lv_obj_set_size(line_meter_2, 100, 100);
	lv_linemeter_set_range(line_meter_2, 0, 4000);
	lv_linemeter_set_value(line_meter_2, 0);
	//lv_linemeter_set_scale(line_meter_1, 240, 240);
    text_area_2 = lv_textarea_create(parent, NULL);
	lv_obj_set_pos(text_area_2, 200, 80);
	lv_obj_set_size(text_area_2, 100, 26);
	lv_textarea_set_text(text_area_2, "6 mA");
    lv_textarea_set_cursor_hidden(text_area_2,true);

    line_meter_3 = lv_linemeter_create(parent, NULL);
	lv_obj_set_pos(line_meter_3, 0, 120);
	lv_obj_set_size(line_meter_3, 100, 100);
	lv_linemeter_set_range(line_meter_3, 0, 20000);
	lv_linemeter_set_value(line_meter_3, 0);
	//lv_linemeter_set_scale(line_meter_1, 240, 240);
    text_area_3 = lv_textarea_create(parent, NULL);
	lv_obj_set_pos(text_area_3, 0, 120+80);
	lv_obj_set_size(text_area_3, 100, 26);
	lv_textarea_set_text(text_area_3, "0 mW");
    lv_textarea_set_cursor_hidden(text_area_3,true);
    //v_textarea_set_style(text_area_2, LV_textarea_STYLE_BG, &style0_text_area_2);
    text_area_4 = lv_textarea_create(parent, NULL);
	lv_obj_set_pos(text_area_4, 130, 180);
	lv_obj_set_size(text_area_4, 160, 26);
     lv_textarea_set_cursor_hidden(text_area_4,true);
   lv_textarea_set_text(text_area_4, "0.00 J");
   lv_obj_t *image_1 = lv_img_create(parent, NULL);
	lv_obj_set_pos(image_1, 108, 116);
	lv_obj_set_size(image_1, 157, 53);
	lv_img_set_src(image_1, &logo);
	//lv_led_set_bright(led_1, 255);
}

void lvgl_mygui_demo(void)
{
    static lv_obj_t * main_page,*bt0,*bt0_txt;
    main_page = lv_page_create(lv_scr_act(), NULL);
    lv_page_set_anim_time(main_page, 30);
    lv_obj_set_size(main_page, LV_HOR_RES , LV_VER_RES);
   // lv_page_set_scrl_layout(main_page, LV_LAYOUT_PRETTY_MID);
    app_create(main_page);
 xTaskCreatePinnedToCore(guiMaintain, "gui-update", 4096*8, NULL, 0, NULL, 0);
  
}