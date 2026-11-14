/**
 * Main UI Screen
 * Pizza Oven interface - Effeuno style
 */

#ifndef UI_MAIN_H
#define UI_MAIN_H

#include <lvgl.h>

// ============================================
// Screen Management
// ============================================

// Create and show main screen
void ui_main_create();

// Destroy the main screen (no-op if it is the active one)
void ui_main_destroy();

// Update UI elements from state (call periodically)
void ui_main_update();

// Get main screen object
lv_obj_t* ui_main_get_screen();

// ============================================
// Event Callbacks (internal)
// ============================================

// Called when power button is pressed
void ui_main_on_power_click(lv_event_t* e);

// Called when a phase gauge is clicked
void ui_main_on_phase_click(lv_event_t* e);

// Called when SET value is clicked
void ui_main_on_set_click(lv_event_t* e);

// Called when PWR value is clicked
void ui_main_on_pwr_click(lv_event_t* e);

// Called when timer controls are clicked
void ui_main_on_timer_click(lv_event_t* e);

// Called when boost button is clicked
void ui_main_on_boost_click(lv_event_t* e);

#endif // UI_MAIN_H
