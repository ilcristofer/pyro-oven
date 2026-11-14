/**
 * Display Driver for WT32-SC01
 * LovyanGFX backend for LVGL
 */

#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <lvgl.h>

// Initialize display and touch drivers for LVGL
void display_driver_init();

// Get the LGFX display instance (for direct access if needed)
void* display_driver_get_display();

#endif // DISPLAY_DRIVER_H
