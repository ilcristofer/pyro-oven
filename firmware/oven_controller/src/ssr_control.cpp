/**
 * SSR Control Driver Implementation
 * Burst firing control for SSR-40DA solid state relays
 */

#include "ssr_control.h"
#include "hardware_pins.h"
#include "oven_state.h"

// ============================================
// Internal state
// ============================================
static unsigned long last_update_time = 0;
static int cycle_position = 0;          // 0-99 position in the 1-second cycle
static bool ssr_cielo_state = false;
static bool ssr_platea_state = false;

// Override per calibrazione
static bool s_override_active = false;
static int s_override_power_cielo = 0;
static int s_override_power_platea = 0;

// ============================================
// Initialize SSR control
// ============================================
void ssr_control_init() {
    // Configure SSR pins as outputs
    pinMode(SSR_CIELO, OUTPUT);
    pinMode(SSR_PLATEA, OUTPUT);

    // Start with SSRs off
    digitalWrite(SSR_CIELO, LOW);
    digitalWrite(SSR_PLATEA, LOW);

    ssr_cielo_state = false;
    ssr_platea_state = false;
    cycle_position = 0;
    last_update_time = millis();

    Serial.println("SSR Control initialized");
    Serial.printf("  SSR_CIELO: GPIO%d\n", SSR_CIELO);
    Serial.printf("  SSR_PLATEA: GPIO%d\n", SSR_PLATEA);
    Serial.printf("  Period: %dms, Update interval: %dms\n", SSR_PERIOD_MS, SSR_UPDATE_INTERVAL);
}

// ============================================
// Update SSR outputs
// ============================================
void ssr_control_update() {
    unsigned long now = millis();

    // Update every SSR_UPDATE_INTERVAL ms
    if (now - last_update_time < SSR_UPDATE_INTERVAL) {
        return;
    }
    last_update_time = now;

    // Advance cycle position (0-99)
    cycle_position++;
    if (cycle_position >= 100) {
        cycle_position = 0;
    }

    // Get effective power levels (0-100%)
    int power_cielo = 0;
    int power_platea = 0;

    if (s_override_active) {
        // Calibrazione: usa valori di override diretti
        power_cielo = s_override_power_cielo;
        power_platea = s_override_power_platea;
    } else if (g_state.power_on && g_state.mode != OvenMode::STANDBY) {
        // Controllo normale: solo se acceso e non in standby
        if (oven_state_resistance_cielo_on()) {
            power_cielo = oven_state_get_effective_power_cielo();
        }
        if (oven_state_resistance_platea_on()) {
            power_platea = oven_state_get_effective_power_platea();
        }
    }

    // Burst firing: SSR is ON if cycle_position < power_percentage
    // Example: 75% power = ON for positions 0-74, OFF for 75-99
    bool cielo_on = (cycle_position < power_cielo);
    bool platea_on = (cycle_position < power_platea);

    // Update outputs only if state changed (reduces GPIO writes)
    if (cielo_on != ssr_cielo_state) {
        ssr_cielo_state = cielo_on;
        digitalWrite(SSR_CIELO, cielo_on ? HIGH : LOW);
    }

    if (platea_on != ssr_platea_state) {
        ssr_platea_state = platea_on;
        digitalWrite(SSR_PLATEA, platea_on ? HIGH : LOW);
    }
}

// ============================================
// Emergency stop - force all SSRs off
// ============================================
void ssr_control_all_off() {
    digitalWrite(SSR_CIELO, LOW);
    digitalWrite(SSR_PLATEA, LOW);
    ssr_cielo_state = false;
    ssr_platea_state = false;
    Serial.println("SSR: Emergency stop - all off");
}

// ============================================
// Diagnostic functions
// ============================================
bool ssr_control_cielo_on() {
    return ssr_cielo_state;
}

bool ssr_control_platea_on() {
    return ssr_platea_state;
}

int ssr_control_get_cycle_position() {
    return cycle_position;
}

// ============================================
// Override per calibrazione
// ============================================
void ssr_control_set_override(bool active, int power_cielo, int power_platea) {
    s_override_active = active;
    s_override_power_cielo = power_cielo;
    s_override_power_platea = power_platea;

    if (!active) {
        // Quando si disattiva override, forza SSR off per sicurezza
        digitalWrite(SSR_CIELO, LOW);
        digitalWrite(SSR_PLATEA, LOW);
        ssr_cielo_state = false;
        ssr_platea_state = false;
    }
}
