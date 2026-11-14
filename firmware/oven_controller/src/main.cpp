/**
 * PYRO - Pizza Oven Controller
 * Hardware: WT32-SC01 + 2x MAX31855
 * Display: 320x480 Portrait
 */

#include <Arduino.h>
#include <lvgl.h>
#include <esp_task_wdt.h>

#include "hardware_pins.h"
#include "display_driver.h"
#include "sensors.h"
#include "oven_state.h"
#include "ui_theme.h"
#include "ui_main.h"
#include "ui_keypad.h"
#include "ui_settings.h"
#include "ui_programs.h"
#include "ui_program_edit.h"
#include "wifi_time.h"
#include "wifi_storage.h"
#include "buzzer.h"
#include "fan.h"
#include "settings_storage.h"
#include "programs_storage.h"
#include "ssr_control.h"
#include "ui_wifi.h"
#include "ui_ota.h"
#include "ota_server.h"
#include "thermal_profile.h"
#include "cooldown.h"
#include "ui_auto.h"
#include "ui_mode.h"
#include "ui_inactivity.h"
#include "schedule.h"

// ============================================
// Timing
// ============================================
static unsigned long last_sensor_read = 0;
static unsigned long last_ui_update = 0;
static unsigned long last_slow_tick = 0;  // 1Hz tick for clock, inactivity

// Watchdog del loop: se il loop si blocca l'SSR resta nell'ultimo stato, che
// puo' essere acceso. Dopo questo tempo il chip si riavvia e i pin tornano bassi
#define LOOP_WDT_TIMEOUT_S  10

// Stato precedente degli errori sonda, per allarmare solo sul fronte di salita
static bool s_prev_error_cielo = false;
static bool s_prev_error_platea = false;

// ============================================
// Arresto di sicurezza (ventola ferma, scheda surriscaldata)
// ============================================
static void safety_shutdown(const char* msg) {
    g_state.error_fan = true;
    strncpy(g_state.error_msg_fan, msg, sizeof(g_state.error_msg_fan) - 1);
    g_state.error_msg_fan[sizeof(g_state.error_msg_fan) - 1] = '\0';
    g_state.power_on = false;
    ssr_control_all_off();
    oven_state_go_standby();
    buzzer_error();
    Serial.printf("SICUREZZA: %s! SSR disabilitati.\n", msg);
}

// ============================================
// Splash Screen
// ============================================
static void show_splash() {
    lv_obj_t* splash = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(splash, COLOR_BG, 0);

    lv_obj_t* title = lv_label_create(splash);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(title, COLOR_ORANGE, 0);
    lv_label_set_text(title, "PYRO");
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -20);

    lv_obj_t* version = lv_label_create(splash);
    lv_obj_set_style_text_font(version, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(version, COLOR_WHITE, 0);
    lv_label_set_text(version, "Pizza Oven Controller");
    lv_obj_align(version, LV_ALIGN_CENTER, 0, 20);

    lv_scr_load(splash);

    // Process LVGL for splash display
    for (int i = 0; i < 50; i++) {
        lv_timer_handler();
        delay(10);
    }
}

// ============================================
// Setup
// ============================================
void setup() {
    Serial.begin(115200);
    Serial.println(F("\n========================================"));
    Serial.println(F("PYRO - Pizza Oven Controller v" FW_VERSION));
    Serial.println(F("========================================"));

    // SSR per primi: le uscite devono essere basse prima di qualsiasi altra
    // inizializzazione (display, NVS, melodia bloccante)
    ssr_control_init();

    // Initialize LVGL
    Serial.println(F("Initializing LVGL..."));
    lv_init();

    // Initialize display driver (LovyanGFX + LVGL)
    Serial.println(F("Initializing display..."));
    display_driver_init();

    // Initialize theme
    Serial.println(F("Initializing theme..."));
    ui_theme_init();

    // Initialize settings (load from NVS)
    Serial.println(F("Initializing settings..."));
    settings_init();

    // Initialize thermal profile (load from NVS)
    Serial.println(F("Initializing thermal profile..."));
    thermal_profile_init();
    cooldown_init();
    schedule_init();  // dopo settings_init: il default della temperatura e' quello SEMPLICE

    // Initialize buzzer (uses settings)
    Serial.println(F("Initializing buzzer..."));
    buzzer_init();

    // Initialize cooling fan
    Serial.println(F("Initializing fan..."));
    fan_init();
    fan_tach_init();  // TACH su GPIO5

    // Show splash screen then play startup melody
    Serial.println(F("Showing splash..."));
    show_splash();
    buzzer_startup();  // Melodia di avvio (bloccante)

    // Initialize sensors
    Serial.println(F("Initializing sensors..."));
    sensors_init();

    // Initialize oven state (default values)
    Serial.println(F("Initializing state..."));
    oven_state_init();

    // Initialize programs (load P0 from NVS - AFTER oven_state_init!)
    Serial.println(F("Initializing programs..."));
    programs_storage_init();

    // Initialize WiFi storage (load credentials from NVS)
    Serial.println(F("Initializing WiFi storage..."));
    wifi_storage_init();

    // Initialize WiFi and NTP
    Serial.println(F("Initializing WiFi..."));
    wifi_time_init();

    // First sensor reading
    Serial.println(F("First sensor read..."));
    sensors_read();

    // Start directly in SEMPLICE mode
    Serial.println(F("Creating UI..."));
    ui_auto_create();

    // Initialize timing
    last_sensor_read = millis();
    last_ui_update = millis();

    // Watchdog sul task del loop (Arduino lo nutre a ogni giro di loop())
    esp_task_wdt_init(LOOP_WDT_TIMEOUT_S, true);
    enableLoopWDT();

    Serial.println(F("========================================"));
    Serial.println(F("Setup complete!"));
    Serial.println(F("========================================\n"));
}

// ============================================
// Loop
// ============================================
void loop() {
    unsigned long now = millis();

    // LVGL timer handler - call frequently!
    lv_timer_handler();

    // Update buzzer (non-blocking sound patterns)
    buzzer_update();

    // Update SSR control (burst firing @ 100Hz)
    ssr_control_update();

    // Update WiFi connection
    wifi_time_update();

    // Web server: auto-start when WiFi connected, handle requests always
    if (wifi_time_is_connected() && !ota_server_is_running()) {
        ota_server_start();
    }
    if (ota_server_is_running()) {
        ota_server_update();
    }

    // Cambio modalita' SEMPLICE/PRO richiesto da touch, web o impostazioni
    // (eseguito qui: distruggere uno schermo dentro la sua callback crasha LVGL)
    ui_mode_process_pending();

    // Update state timers
    oven_state_update_timer();
    schedule_update();  // subito dopo: vede la scadenza del timer prima di un tocco
    oven_state_update_boost();
    oven_state_update_animation();

    // 1Hz tick: clock and inactivity (no need to check 200x/sec)
    if (now - last_slow_tick >= 1000) {
        last_slow_tick = now;
        oven_state_update_clock();
        oven_state_check_inactivity();
        ui_inactivity_update();
    }

    // Read sensors periodically
    if (now - last_sensor_read >= SENSOR_READ_INTERVAL) {
        last_sensor_read = now;
        sensors_read();
        oven_state_update_recovery();         // Infornata rilevata dal crollo del cielo
        oven_state_update_mode();
        oven_state_update_effective_power();  // Refresh cached power after sensor read
        fan_update(g_state.temp_ambient);  // Adjust fan speed based on box temp

        // Fan TACH monitoring
        fan_tach_update();
        g_state.fan_rpm = fan_tach_get_rpm();

        // Thermal profile auto-learning
        thermal_profile_record();
        cooldown_record();                    // Stima del raffreddamento a forno spento

        // TACH failure check - spegne forno se ventola ferma (disattivabile da impostazioni)
        if (settings_get_tach_enabled() && fan_tach_check_failure() && !g_state.error_fan) {
            safety_shutdown("Ventola ferma");
        }

        // Scheda troppo calda: vale anche con il controllo della ventola
        // disattivato, o con la ventola che gira ma non basta
        if (g_state.power_on && g_state.temp_ambient >= BOX_TEMP_MAX && !g_state.error_fan) {
            safety_shutdown("Box surriscald.");
        }

        // Sonda guasta a forno acceso: la sua resistenza e' gia' spenta, ma
        // senza un allarme ci si accorgeva solo dal forno che non scalda
        bool new_err = (g_state.error_cielo && !s_prev_error_cielo) ||
                       (g_state.error_platea && !s_prev_error_platea);
        if (new_err && g_state.power_on) {
            buzzer_error();
            Serial.printf("SICUREZZA: errore sonda cielo=%s platea=%s\n",
                          g_state.error_msg_cielo, g_state.error_msg_platea);
        }
        s_prev_error_cielo = g_state.error_cielo;
        s_prev_error_platea = g_state.error_platea;
    }

    // Update UI periodically (not every frame)
    if (now - last_ui_update >= 100) {  // 10 Hz
        last_ui_update = now;

        // Update overlay screens (if visible)
        ui_wifi_update();
        ui_ota_update();
        ui_settings_update();

        // Auto mode screen update
        if (ui_auto_is_visible() &&
                 !ui_keypad_is_visible() && !ui_settings_is_visible()) {
            ui_auto_update();
        }
        // Main screen update (manual mode)
        else if (!ui_keypad_is_visible() && !ui_settings_is_visible() &&
                 !ui_programs_is_visible() && !ui_program_edit_is_visible() &&
                 !ui_wifi_is_visible() && !ui_ota_is_visible()) {
            ui_main_update();
        }
    }

    // Small delay to prevent watchdog issues
    delay(5);
}
