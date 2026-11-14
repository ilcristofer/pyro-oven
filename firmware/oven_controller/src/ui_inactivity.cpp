/**
 * Preavviso di spegnimento automatico per inattivita'
 */

#include "ui_inactivity.h"
#include "ui_theme.h"
#include "hardware_pins.h"
#include "oven_state.h"
#include "buzzer.h"
#include <lvgl.h>

static lv_obj_t* s_box = nullptr;
static lv_obj_t* s_lbl = nullptr;
static bool s_warned = false;   // allarme gia' suonato per questo preavviso
static int s_prev_shown_s = -1;

static void on_box_click(lv_event_t* e) {
    // Il tocco e' gia' attivita' (driver touch): qui serve solo a non far
    // arrivare il tocco al pulsante sottostante, e a chiudere subito
    oven_state_register_activity();
    buzzer_beep();
    if (s_box) lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
}

static void create_box() {
    s_box = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_box, SCREEN_WIDTH - 30, 130);
    lv_obj_align(s_box, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_color(s_box, COLOR_RED_DARK, 0);
    lv_obj_set_style_bg_opa(s_box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(s_box, COLOR_ORANGE, 0);
    lv_obj_set_style_border_width(s_box, 3, 0);
    lv_obj_set_style_radius(s_box, 14, 0);
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_box, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_box, on_box_click, LV_EVENT_CLICKED, NULL);

    s_lbl = lv_label_create(s_box);
    lv_obj_set_width(s_lbl, SCREEN_WIDTH - 60);
    lv_obj_set_style_text_font(s_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_lbl, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(s_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_lbl);
}

void ui_inactivity_update() {
    int rem = oven_state_inactivity_remaining_s();
    bool show = rem > 0 && rem <= INACTIVITY_WARN_S;

    if (!show) {
        s_warned = false;
        s_prev_shown_s = -1;
        if (s_box) lv_obj_add_flag(s_box, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (!s_box) create_box();

    if (!s_warned) {
        s_warned = true;
        buzzer_alert();
        Serial.println("Inattivita': preavviso di spegnimento");
    }

    if (rem != s_prev_shown_s) {
        s_prev_shown_s = rem;
        char buf[96];
        snprintf(buf, sizeof(buf), "SPEGNIMENTO AUTOMATICO\ntra %d:%02d\nTocca per continuare",
                 rem / 60, rem % 60);
        lv_label_set_text(s_lbl, buf);
    }
    lv_obj_clear_flag(s_box, LV_OBJ_FLAG_HIDDEN);
}
