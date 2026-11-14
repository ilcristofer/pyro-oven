/**
 * SSR Control Driver
 * Burst firing control for SSR-40DA solid state relays
 *
 * Uses zero-crossing burst firing with 1 second period
 * for smooth power control on resistive heating elements.
 */

#ifndef SSR_CONTROL_H
#define SSR_CONTROL_H

#include <Arduino.h>

// ============================================
// SSR Control Configuration
// ============================================
#define SSR_PERIOD_MS       1000    // Burst firing period (1 second)
#define SSR_UPDATE_INTERVAL 10      // Update every 10ms (100 steps per period)

// ============================================
// SSR Control Functions
// ============================================

/**
 * Initialize SSR control pins
 * Must be called once at startup
 */
void ssr_control_init();

/**
 * Update SSR outputs based on current power settings
 * Must be called frequently (every 10ms) from main loop
 * Uses oven_state_get_effective_power_cielo/platea() for power levels
 */
void ssr_control_update();

/**
 * Force both SSRs off (emergency stop)
 */
void ssr_control_all_off();

/**
 * Get current SSR state for diagnostics
 */
bool ssr_control_cielo_on();
bool ssr_control_platea_on();

/**
 * Get current duty cycle position (0-99)
 */
int ssr_control_get_cycle_position();

/**
 * Set override mode for calibration
 * When active, bypasses normal g_state-based control
 * @param active  true to enable override, false to disable
 * @param power_cielo  power % for cielo (0-100) when override active
 * @param power_platea power % for platea (0-100) when override active
 */
void ssr_control_set_override(bool active, int power_cielo, int power_platea);

#endif // SSR_CONTROL_H
