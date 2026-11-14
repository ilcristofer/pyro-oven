/**
 * UI Theme - Effeuno style dark theme
 * Colors and layout constants matching the original Effeuno interface
 */

#ifndef UI_THEME_H
#define UI_THEME_H

#include <lvgl.h>

// ============================================
// Color definitions - Effeuno palette
// ============================================
#define COLOR_BG              lv_color_hex(0x000000)  // Pure black background
#define COLOR_HEADER_BG       lv_color_hex(0x1A1A1A)  // Dark gray header/footer
#define COLOR_PANEL_BG        lv_color_hex(0x000000)  // Panel background (same as bg)
#define COLOR_BUTTON_BG       lv_color_hex(0x1A1A1A)  // Button background

// Primary colors
#define COLOR_WHITE           lv_color_hex(0xFFFFFF)  // White text
#define COLOR_BLACK           lv_color_hex(0x000000)  // Black
#define COLOR_GRAY            lv_color_hex(0x808080)  // Gray for inactive/labels
#define COLOR_DARK_GRAY       lv_color_hex(0x333333)  // Dark gray for borders/inactive
#define COLOR_LIGHT_GRAY      lv_color_hex(0xC0C0C0)  // Light gray

// Status colors
#define COLOR_GREEN           lv_color_hex(0x00FF00)  // Bright green - READY, Play
#define COLOR_GREEN_DARK      lv_color_hex(0x2E7D32)  // Dark green - ACCENDI button (auto mode)
#define COLOR_GREEN_READY     lv_color_hex(0x388E3C)  // Medium green - PRONTO status bar (auto mode)
#define COLOR_RED             lv_color_hex(0xFF0000)  // Bright red - Stop, Error, Boost active
#define COLOR_RED_DARK        lv_color_hex(0xD32F2F)  // Dark red - SPEGNI button (auto mode)
#define COLOR_ORANGE          lv_color_hex(0xFF8C00)  // Orange - Preheat, Reset
#define COLOR_ORANGE_DARK     lv_color_hex(0xCC7000)  // Dark orange - button pressed
#define COLOR_YELLOW          lv_color_hex(0xFFFF00)  // Yellow - Timer duration
#define COLOR_BORDEAUX        lv_color_hex(0x800020)  // Bordeaux - Cooking

// Temperature bar gradient
#define COLOR_TEMP_LOW        lv_color_hex(0xFF0000)  // Red (cold/low)
#define COLOR_TEMP_MID        lv_color_hex(0xFFFF00)  // Yellow (heating)
#define COLOR_TEMP_HIGH       lv_color_hex(0x00FF00)  // Green (at target)

// Effeuno specific accent colors
#define COLOR_CIELO           lv_color_hex(0x00BFFF)  // Sky blue for cielo (not used much in Effeuno)
#define COLOR_PLATEA          lv_color_hex(0xFF6B00)  // Orange for platea (not used much in Effeuno)

// ============================================
// Screen dimensions (WT32-SC01: 320x480)
// ============================================
#define SCREEN_WIDTH          320
#define SCREEN_HEIGHT         480

// ============================================
// Layout constants - Effeuno style
// Vertical layout from top to bottom
// ============================================

// Header section (date/time + power button)
#define HEADER_Y              0
#define HEADER_HEIGHT         35

// Phase gauges section (3 circular indicators)
#define PHASES_Y              35
#define PHASES_HEIGHT         105

// Status bar (PROG. | Name | Status)
#define STATUS_Y              130
#define STATUS_HEIGHT         28

// Temperature display section (big temp + bar)
#define TEMP_Y                168
#define TEMP_HEIGHT           90

// SET/Power section (set-points and power percentages)
#define SETPWR_Y              258
#define SETPWR_HEIGHT         62

// Controls section (restart, play, timer, save)
#define CONTROLS_Y            320
#define CONTROLS_HEIGHT       55

// Function buttons section (2x4 grid)
#define BUTTONS_Y             375
#define BUTTONS_HEIGHT        105

// ============================================
// Component dimensions
// ============================================

// Phase gauge
#define GAUGE_SIZE            88
#define GAUGE_SPACING         106

// Function buttons
#define FUNC_BTN_WIDTH        72
#define FUNC_BTN_HEIGHT       46
#define FUNC_BTN_SPACING      6

// Control buttons
#define CTRL_BTN_SIZE         50

// ============================================
// Style declarations
// ============================================
extern lv_style_t style_bg;
extern lv_style_t style_header;
extern lv_style_t style_panel;
extern lv_style_t style_button;
extern lv_style_t style_button_pressed;
extern lv_style_t style_button_active;
extern lv_style_t style_label_large;
extern lv_style_t style_label_medium;
extern lv_style_t style_label_small;

// ============================================
// Theme functions
// ============================================

// Initialize all styles
void ui_theme_init();

// Apply dark background to screen
void ui_theme_apply_bg(lv_obj_t* obj);

#endif // UI_THEME_H
