/**
 * Cooling Fan Driver Implementation
 */

#include "fan.h"
#include "hardware_pins.h"

static uint8_t current_speed = 0;

// TACH monitoring
static volatile uint32_t g_fan_pulse_count = 0;
static uint16_t g_fan_rpm = 0;
static uint8_t g_fan_error_count = 0;
static unsigned long g_fan_last_speed_change = 0;
static bool g_fan_tach_initialized = false;

static void IRAM_ATTR fan_tach_isr() {
    g_fan_pulse_count++;
}

void fan_init() {
    // PWM su GPIO0 - non collegato per ora, ventola a 12V fisso
    ledcSetup(FAN_PWM_CHANNEL, FAN_PWM_FREQ, FAN_PWM_RES);
    ledcAttachPin(FAN_PIN, FAN_PWM_CHANNEL);
    ledcWrite(FAN_PWM_CHANNEL, 0);

    Serial.printf("Fan PWM initialized on GPIO%d (ch%d) - non collegato\n", FAN_PIN, FAN_PWM_CHANNEL);
}

void fan_update(int temp_box) {
    uint8_t speed;

    if (temp_box <= FAN_TEMP_MIN) {
        // Below minimum threshold: fan at idle (silent cooling)
        speed = FAN_PWM_IDLE;
    } else if (temp_box >= FAN_TEMP_MAX) {
        // Above maximum threshold: fan at 100%
        speed = FAN_PWM_MAX;
    } else {
        // Linear ramp between thresholds
        // Map temp_box from [FAN_TEMP_MIN, FAN_TEMP_MAX] to [IDLE, MAX]
        speed = map(temp_box, FAN_TEMP_MIN, FAN_TEMP_MAX, FAN_PWM_IDLE, FAN_PWM_MAX);
    }

    fan_set_speed(speed);
}

void fan_set_speed(uint8_t speed) {
    if (speed != current_speed) {
        g_fan_last_speed_change = millis();
        Serial.printf("Fan PWM: %d -> %d (GPIO%d, ch%d)\n",
                      current_speed, speed, FAN_PIN, FAN_PWM_CHANNEL);
    }
    current_speed = speed;
    ledcWrite(FAN_PWM_CHANNEL, speed);
}

// ============================================
// TACH monitoring
// ============================================

void fan_tach_init() {
    pinMode(FAN_TACH_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FAN_TACH_PIN), fan_tach_isr, FALLING);
    g_fan_tach_initialized = true;
    g_fan_last_speed_change = millis();
    Serial.printf("Fan TACH initialized on GPIO%d\n", FAN_TACH_PIN);
}

void fan_tach_update() {
    if (!g_fan_tach_initialized) return;

    // Atomic read and reset of pulse count
    noInterrupts();
    uint32_t pulses = g_fan_pulse_count;
    g_fan_pulse_count = 0;
    interrupts();

    // RPM = (pulses / FAN_PULSES_PER_REV) * (60000 / FAN_TACH_INTERVAL)
    // Simplified: RPM = pulses * 60000 / (FAN_PULSES_PER_REV * FAN_TACH_INTERVAL)
    // With PULSES_PER_REV=2, INTERVAL=2000: RPM = pulses * 30000 / 2000 = pulses * 15
    // Wait: 60000 / (2 * 2000) = 15. So RPM = pulses * 15
    // But plan says RPM = pulses * 30 which assumes interval=1000. Let's compute correctly:
    g_fan_rpm = (uint16_t)(pulses * 60000UL / (FAN_PULSES_PER_REV * FAN_TACH_INTERVAL));
}

uint16_t fan_tach_get_rpm() {
    return g_fan_rpm;
}

bool fan_tach_check_failure() {
    if (!g_fan_tach_initialized) return false;

    // Only check if fan is being driven above minimum threshold
    if (current_speed < FAN_PWM_MIN_FOR_CHECK) {
        g_fan_error_count = 0;
        return false;
    }

    // Grace period after speed change (allow spinup)
    if (millis() - g_fan_last_speed_change < FAN_STARTUP_GRACE_MS) {
        return false;
    }

    // Check RPM against threshold
    if (g_fan_rpm < FAN_RPM_MIN_THRESHOLD) {
        g_fan_error_count++;
    } else {
        g_fan_error_count = 0;
    }

    return (g_fan_error_count >= FAN_FAILURE_COUNT);
}
