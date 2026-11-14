/**
 * Cooling Fan Driver (PWM 4-pin PC fan)
 * Controls box cooling based on ambient temperature
 */

#ifndef FAN_H
#define FAN_H

#include <Arduino.h>

// Temperature thresholds for fan curve (°C)
#define FAN_TEMP_MIN    35      // Below this: fan off
#define FAN_TEMP_MAX    55      // Above this: fan 100%

// PWM limits
#define FAN_PWM_MIN     0       // Minimum duty (off)
#define FAN_PWM_IDLE    64      // Idle duty (~25%, silent cooling)
#define FAN_PWM_MAX     255     // Maximum duty (100%)

/**
 * Initialize fan PWM
 */
void fan_init();

/**
 * Update fan speed based on box temperature
 * Call this periodically (e.g., after sensor read)
 * @param temp_box Current box/ambient temperature in °C
 */
void fan_update(int temp_box);

/**
 * Set fan speed manually (0-255)
 * @param speed PWM duty cycle (0=off, 255=max)
 */
void fan_set_speed(uint8_t speed);

/**
 * Initialize TACH input interrupt for RPM reading
 */
void fan_tach_init();

/**
 * Update RPM calculation (call every FAN_TACH_INTERVAL)
 */
void fan_tach_update();

/**
 * Get current RPM
 */
uint16_t fan_tach_get_rpm();

/**
 * Check for fan failure (true = fan stopped while it should be running)
 */
bool fan_tach_check_failure();

#endif // FAN_H
