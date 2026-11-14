/**
 * WiFi Configuration Screen
 * Configure WiFi SSID and password
 */

#ifndef UI_WIFI_H
#define UI_WIFI_H

#include <lvgl.h>

// Show WiFi configuration screen
void ui_wifi_show();

// Hide WiFi configuration screen
void ui_wifi_hide();

// Check visibility
bool ui_wifi_is_visible();

// Update WiFi status display (call in loop)
void ui_wifi_update();

#endif // UI_WIFI_H
