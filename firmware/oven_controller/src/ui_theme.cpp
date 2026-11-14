/**
 * UI Theme Implementation - Effeuno style
 */

#include <Arduino.h>
#include "ui_theme.h"

// ============================================
// Style instances
// ============================================
lv_style_t style_bg;
lv_style_t style_header;
lv_style_t style_panel;
lv_style_t style_button;
lv_style_t style_button_pressed;
lv_style_t style_button_active;
lv_style_t style_label_large;
lv_style_t style_label_medium;
lv_style_t style_label_small;

// ============================================
// Initialize theme
// ============================================
void ui_theme_init() {
    // Background style (pure black)
    lv_style_init(&style_bg);
    lv_style_set_bg_color(&style_bg, COLOR_BG);
    lv_style_set_bg_opa(&style_bg, LV_OPA_COVER);

    // Header style (dark gray)
    lv_style_init(&style_header);
    lv_style_set_bg_color(&style_header, COLOR_HEADER_BG);
    lv_style_set_bg_opa(&style_header, LV_OPA_COVER);
    lv_style_set_pad_all(&style_header, 4);

    // Panel style (black, no border)
    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, COLOR_PANEL_BG);
    lv_style_set_bg_opa(&style_panel, LV_OPA_COVER);
    lv_style_set_radius(&style_panel, 0);
    lv_style_set_border_width(&style_panel, 0);
    lv_style_set_pad_all(&style_panel, 0);

    // Button base style (Effeuno style - dark with white/gray border)
    lv_style_init(&style_button);
    lv_style_set_bg_color(&style_button, COLOR_BUTTON_BG);
    lv_style_set_bg_opa(&style_button, LV_OPA_COVER);
    lv_style_set_radius(&style_button, 6);
    lv_style_set_border_width(&style_button, 1);
    lv_style_set_border_color(&style_button, COLOR_GRAY);
    lv_style_set_text_color(&style_button, COLOR_WHITE);
    lv_style_set_pad_all(&style_button, 4);

    // Button pressed style
    lv_style_init(&style_button_pressed);
    lv_style_set_bg_color(&style_button_pressed, COLOR_DARK_GRAY);
    lv_style_set_border_color(&style_button_pressed, COLOR_WHITE);
    lv_style_set_border_width(&style_button_pressed, 2);

    // Button active style (colored border when active)
    lv_style_init(&style_button_active);
    lv_style_set_border_width(&style_button_active, 2);

    // Large label (temperature display - 48pt)
    lv_style_init(&style_label_large);
    lv_style_set_text_color(&style_label_large, COLOR_WHITE);
    lv_style_set_text_font(&style_label_large, &lv_font_montserrat_48);

    // Medium label (24pt)
    lv_style_init(&style_label_medium);
    lv_style_set_text_color(&style_label_medium, COLOR_WHITE);
    lv_style_set_text_font(&style_label_medium, &lv_font_montserrat_24);

    // Small label (14pt)
    lv_style_init(&style_label_small);
    lv_style_set_text_color(&style_label_small, COLOR_WHITE);
    lv_style_set_text_font(&style_label_small, &lv_font_montserrat_14);

    Serial.println("UI theme initialized (Effeuno style)");
}

// ============================================
// Apply background
// ============================================
void ui_theme_apply_bg(lv_obj_t* obj) {
    lv_obj_add_style(obj, &style_bg, 0);
}
