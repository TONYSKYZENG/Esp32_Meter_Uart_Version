
#include "lvgl.h"
#include <stdint.h>
#include "esp_err.h"
#include <string.h>
#include <stdatomic.h>
#include "esp_timer.h"
lv_obj_t *window_home;
void lvgl_demo_ui(lv_display_t *disp)
{
    window_home = lv_display_get_screen_active(disp);
    lv_disp_set_rotation(disp, LV_DISP_ROTATION_270);
    
    /*Button event*/
   
    {
            // btn_go
            lv_obj_t *obj = lv_button_create(window_home);
            //btn = obj;
            lv_obj_set_pos(obj, 0, 180);
            lv_obj_set_size(obj, 71, 26);
            {
                lv_obj_t *parent_obj = obj;
                {
                    lv_obj_t *obj = lv_label_create(parent_obj);
                    lv_obj_set_pos(obj, 1, 0);
                    lv_obj_set_size(obj, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
                    lv_obj_set_style_align(obj, LV_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
                    lv_label_set_text_static(obj, LV_SYMBOL_AUDIO" Go");
                }
            }
            //lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, disp);
    }
    

}