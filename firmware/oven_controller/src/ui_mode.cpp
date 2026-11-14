/**
 * UI Mode Switching Implementation
 */

#include "ui_mode.h"
#include "ui_main.h"
#include "ui_auto.h"
#include "buzzer.h"
#include "ui_keypad.h"
#include "ui_settings.h"
#include "ui_programs.h"
#include "ui_program_edit.h"
#include "ui_wifi.h"
#include "ui_ota.h"
#include <Arduino.h>

// ============================================
// State
// ============================================
static int s_pending = 0;            // 0 = nessuno, 1 = -> PRO, 2 = -> SEMPLICE
static uint32_t s_press_start = 0;   // tick inizio pressione sul MENU
static bool s_long_fired = false;    // pressione lunga gia' riconosciuta

// ============================================
// Query / Request
// ============================================
bool ui_mode_is_pro() {
    return !ui_auto_is_visible();
}

void ui_mode_request_switch(bool to_pro) {
    if (to_pro && !ui_mode_is_pro()) {
        s_pending = 1;
    } else if (!to_pro && ui_mode_is_pro()) {
        s_pending = 2;
    }
}

void ui_mode_request_toggle() {
    ui_mode_request_switch(!ui_mode_is_pro());
}

// ============================================
// Execution (main loop only)
// ============================================
void ui_mode_process_pending() {
    if (s_pending == 0) return;

    // Con un overlay aperto (tastierino, menu, programmi, WiFi, OTA) il cambio
    // aspetta che venga chiuso: i loro flag di visibilita' restavano veri sul
    // nuovo schermo e il main loop smetteva di aggiornarlo
    if (ui_keypad_is_visible() || ui_settings_is_visible() || ui_programs_is_visible() ||
        ui_program_edit_is_visible() || ui_wifi_is_visible() || ui_ota_is_visible()) {
        return;
    }

    // IMPORTANTE: creare prima il nuovo schermo (diventa attivo), poi
    // distruggere il vecchio - cancellare lo schermo attivo fa crashare LVGL
    if (s_pending == 1 && !ui_mode_is_pro()) {
        ui_main_create();     // crea e carica lo schermo PRO
        ui_auto_destroy();    // sicuro: screen_auto non e' piu' attivo
        Serial.println("Mode: passaggio a PRO");
    } else if (s_pending == 2 && ui_mode_is_pro()) {
        ui_auto_create();     // crea e carica lo schermo SEMPLICE
        ui_main_destroy();    // sicuro: screen_main non e' piu' attivo
        Serial.println("Mode: passaggio a SEMPLICE");
    }
    s_pending = 0;
}

// ============================================
// Long press sul pulsante MENU
// ============================================
static void menu_long_press_cb(lv_event_t* e) {
    switch (lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:
            s_press_start = lv_tick_get();
            s_long_fired = false;
            break;

        case LV_EVENT_LONG_PRESSED_REPEAT:
            // LVGL invia il primo LONG_PRESSED dopo 400ms: aspettiamo la
            // soglia piu' lunga per non cambiare modalita' per sbaglio
            if (!s_long_fired && lv_tick_elaps(s_press_start) >= UI_MODE_LONG_PRESS_MS) {
                s_long_fired = true;
                buzzer_confirm();  // feedback: pressione lunga riconosciuta
            }
            break;

        case LV_EVENT_PRESS_LOST:
            // Dito uscito dal pulsante: pressione annullata
            s_long_fired = false;
            break;

        case LV_EVENT_RELEASED:
            // Cambio al rilascio: lo schermo che contiene questo pulsante
            // viene distrutto, farlo con il dito ancora premuto attiverebbe
            // il pulsante sottostante del nuovo schermo
            if (s_long_fired) ui_mode_request_toggle();
            break;

        default:
            break;
    }
}

void ui_mode_attach_menu_long_press(lv_obj_t* btn) {
    lv_obj_add_event_cb(btn, menu_long_press_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(btn, menu_long_press_cb, LV_EVENT_LONG_PRESSED_REPEAT, NULL);
    lv_obj_add_event_cb(btn, menu_long_press_cb, LV_EVENT_PRESS_LOST, NULL);
    lv_obj_add_event_cb(btn, menu_long_press_cb, LV_EVENT_RELEASED, NULL);
}

bool ui_mode_consume_long_press() {
    if (!s_long_fired) return false;
    s_long_fired = false;
    return true;
}
