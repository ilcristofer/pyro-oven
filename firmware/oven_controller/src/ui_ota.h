/**
 * OTA Update Screen
 * Shows update progress and IP address
 */

#ifndef UI_OTA_H
#define UI_OTA_H

#include <lvgl.h>

// Show OTA update screen (starts WiFi and server)
void ui_ota_show();

// Hide OTA update screen (stops server)
void ui_ota_hide();

// Check visibility
bool ui_ota_is_visible();

// Update display (call in loop)
void ui_ota_update();

#endif // UI_OTA_H
