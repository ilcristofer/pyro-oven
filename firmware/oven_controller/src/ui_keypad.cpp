/**
 * Numeric Keypad Implementation
 */

#include <Arduino.h>
#include "ui_keypad.h"
#include "ui_theme.h"
#include "ui_main.h"
#include "hardware_pins.h"
#include "buzzer.h"
#include "programs_storage.h"
#include "oven_state.h"
#include "ui_settings.h"

// ============================================
// Static variables
// ============================================
static lv_obj_t* screen_keypad = nullptr;
static lv_obj_t* lbl_title = nullptr;
static lv_obj_t* lbl_value = nullptr;
static lv_obj_t* btnm_keypad = nullptr;

static int* edit_target = nullptr;
static int min_value = 0;
static int max_value = 500;
static const char* unit_suffix = nullptr;  // unita' esplicita (nullptr = dedotta dal range)
static String input_buffer = "";
static bool keypad_visible = false;

// Modalita' tempo: si digita come sul microonde, le ultime due cifre sono i
// secondi ("130" = 1:30, "45" = 0:45). Le pizze cuociono in 60-90 secondi:
// un timer a minuti interi non serviva
static bool s_time_mode = false;
static int* edit_min = nullptr;
static int* edit_sec = nullptr;
static lv_obj_t* cont_presets = nullptr;

// Variante orologio (ora del giorno hh:mm): stesse cifre, niente preset,
// ore fino a 23
static bool s_clock_mode = false;

// Preset di tempo (secondi): si sommano al valore sul display (15:00 poi
// 5:00 = 20:00), OK conferma. Le cifre digitate dopo iniziano un valore nuovo
static const int TIME_PRESETS[] = {60, 300, 600, 900};
static bool s_preset_sum = false;
#define TIME_PRESET_COUNT (sizeof(TIME_PRESETS) / sizeof(TIME_PRESETS[0]))

// Button map for keypad
static const char* btnm_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    "C", "0", "OK", ""
};

// ============================================
// Forward declarations
// ============================================
static void keypad_event_cb(lv_event_t* e);
static void update_display();
static void ensure_screen();
static void preset_event_cb(lv_event_t* e);

// ============================================
// Show keypad
// ============================================
void ui_keypad_show(int* target, const char* title, int min_val, int max_val,
                    const char* unit) {
    edit_target = target;
    min_value = min_val;
    max_value = max_val;
    unit_suffix = unit;
    s_time_mode = false;
    s_clock_mode = false;
    edit_min = nullptr;
    edit_sec = nullptr;
    input_buffer = "";
    keypad_visible = true;

    ensure_screen();
    lv_obj_add_flag(cont_presets, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_title, title);
    update_display();
    lv_scr_load(screen_keypad);
    Serial.printf("Keypad shown: %s\n", title);
}

void ui_keypad_show_time(int* minutes, int* seconds, const char* title) {
    edit_target = nullptr;
    edit_min = minutes;
    edit_sec = seconds;
    s_time_mode = true;
    s_clock_mode = false;
    unit_suffix = nullptr;
    input_buffer = "";
    s_preset_sum = false;
    keypad_visible = true;

    ensure_screen();
    lv_obj_clear_flag(cont_presets, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lbl_title, title);
    update_display();
    lv_scr_load(screen_keypad);
    Serial.printf("Keypad (tempo) shown: %s\n", title);
}

void ui_keypad_show_clock(int* hour, int* minute, const char* title) {
    ui_keypad_show_time(hour, minute, title);
    s_clock_mode = true;
    lv_obj_add_flag(cont_presets, LV_OBJ_FLAG_HIDDEN);
    update_display();
}

// ============================================
// Create screen (once)
// ============================================
static void ensure_screen() {
    if (screen_keypad) return;

    screen_keypad = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_keypad, COLOR_BG, 0);
    lv_obj_clear_flag(screen_keypad, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lbl_title = lv_label_create(screen_keypad);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_WHITE, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 20);

    // Value display
    lv_obj_t* value_cont = lv_obj_create(screen_keypad);
    lv_obj_set_size(value_cont, SCREEN_WIDTH - 60, 60);
    lv_obj_align(value_cont, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_set_style_bg_color(value_cont, COLOR_PANEL_BG, 0);
    lv_obj_set_style_radius(value_cont, 10, 0);
    lv_obj_set_style_border_width(value_cont, 0, 0);
    lv_obj_clear_flag(value_cont, LV_OBJ_FLAG_SCROLLABLE);

    lbl_value = lv_label_create(value_cont);
    lv_obj_set_style_text_font(lbl_value, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_value, COLOR_YELLOW, 0);
    lv_obj_center(lbl_value);

    // Preset di tempo (solo in modalita' tempo), tra il valore e i tasti
    cont_presets = lv_obj_create(screen_keypad);
    lv_obj_remove_style_all(cont_presets);
    lv_obj_set_size(cont_presets, SCREEN_WIDTH - 40, 44);
    lv_obj_set_pos(cont_presets, 20, 128);
    lv_obj_clear_flag(cont_presets, LV_OBJ_FLAG_SCROLLABLE);
    int gap = 8;
    int pw = (SCREEN_WIDTH - 40 - (TIME_PRESET_COUNT - 1) * gap) / TIME_PRESET_COUNT;
    for (int i = 0; i < (int)TIME_PRESET_COUNT; i++) {
        lv_obj_t* b = lv_btn_create(cont_presets);
        lv_obj_set_size(b, pw, 44);
        lv_obj_set_pos(b, i * (pw + gap), 0);
        lv_obj_set_style_bg_color(b, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, COLOR_ORANGE, 0);
        lv_obj_set_style_radius(b, 10, 0);
        lv_obj_add_event_cb(b, preset_event_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)TIME_PRESETS[i]);
        lv_obj_t* l = lv_label_create(b);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(l, COLOR_WHITE, 0);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d:%02d", TIME_PRESETS[i] / 60, TIME_PRESETS[i] % 60);
        lv_label_set_text(l, buf);
        lv_obj_center(l);
    }

    // Button matrix
    btnm_keypad = lv_btnmatrix_create(screen_keypad);
    lv_btnmatrix_set_map(btnm_keypad, btnm_map);
    lv_obj_set_size(btnm_keypad, SCREEN_WIDTH - 40, 280);
    lv_obj_align(btnm_keypad, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Style the button matrix
    lv_obj_set_style_bg_color(btnm_keypad, COLOR_BG, 0);
    lv_obj_set_style_border_width(btnm_keypad, 0, 0);
    lv_obj_set_style_pad_all(btnm_keypad, 5, 0);

    // Button style
    lv_obj_set_style_bg_color(btnm_keypad, COLOR_BUTTON_BG, LV_PART_ITEMS);
    lv_obj_set_style_bg_opa(btnm_keypad, LV_OPA_COVER, LV_PART_ITEMS);
    lv_obj_set_style_radius(btnm_keypad, 10, LV_PART_ITEMS);
    lv_obj_set_style_border_width(btnm_keypad, 1, LV_PART_ITEMS);
    lv_obj_set_style_border_color(btnm_keypad, COLOR_WHITE, LV_PART_ITEMS);
    lv_obj_set_style_text_color(btnm_keypad, COLOR_WHITE, LV_PART_ITEMS);
    lv_obj_set_style_text_font(btnm_keypad, &lv_font_montserrat_24, LV_PART_ITEMS);

    // Pressed style
    lv_obj_set_style_bg_color(btnm_keypad, COLOR_GRAY, LV_PART_ITEMS | LV_STATE_PRESSED);

    lv_obj_add_event_cb(btnm_keypad, keypad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

// ============================================
// Conferma di un tempo (tasti o preset)
// ============================================
static void commit_time(int total_s) {
    if (edit_min) *edit_min = total_s / 60;
    if (edit_sec) *edit_sec = total_s % 60;
    buzzer_confirm();
    Serial.printf("Keypad: tempo %d:%02d\n", total_s / 60, total_s % 60);

    // Se siamo in programma manuale (P0), salva automaticamente
    if (g_state.program_number == PROGRAM_MANUAL) {
        programs_save_manual_params();
    }
    ui_keypad_hide();
}

static void preset_event_cb(lv_event_t* e) {
    // Cifre microonde "mmss" -> secondi (vale anche "90" = 1:30)
    int cur = 0;
    if (input_buffer.length() > 0) {
        int n = input_buffer.toInt();
        cur = (n / 100) * 60 + n % 100;
    }
    int t = cur + (int)(intptr_t)lv_event_get_user_data(e);
    if (t > 99 * 60 + 59) t = 99 * 60 + 59;

    char buf[8];
    snprintf(buf, sizeof(buf), "%d", (t / 60) * 100 + t % 60);
    input_buffer = buf;
    s_preset_sum = true;
    buzzer_beep();
    update_display();
}

// ============================================
// Hide keypad
// ============================================
void ui_keypad_hide() {
    keypad_visible = false;
    edit_target = nullptr;

    // Check if we should return to program edit screen
    extern bool ui_program_edit_is_visible();
    extern void ui_program_edit_refresh();

    if (ui_program_edit_is_visible()) {
        ui_program_edit_refresh();
    } else if (ui_settings_is_visible()) {
        // Aperto dalle impostazioni: si torna li', non alla home (che restava
        // ferma finche' le impostazioni risultavano ancora aperte)
        ui_settings_return();
    } else {
        // Return to the correct screen (auto or main)
        extern bool ui_auto_is_visible();
        extern lv_obj_t* ui_auto_get_screen();
        if (ui_auto_is_visible()) {
            lv_scr_load(ui_auto_get_screen());
        } else {
            lv_scr_load(ui_main_get_screen());
        }
    }
    Serial.println("Keypad hidden");
}

// ============================================
// Check visibility
// ============================================
bool ui_keypad_is_visible() {
    return keypad_visible;
}

// ============================================
// Update display
// ============================================
static void update_display() {
    if (!lbl_value) return;

    lv_obj_set_style_text_color(lbl_value, COLOR_YELLOW, 0);

    if (s_time_mode) {
        if (input_buffer.length() == 0) {
            lv_label_set_text(lbl_value, s_clock_mode ? "--:--" : "-:--");
        } else {
            int n = input_buffer.toInt();
            char buf[12];
            snprintf(buf, sizeof(buf), s_clock_mode ? "%02d:%02d" : "%d:%02d", n / 100, n % 100);
            lv_label_set_text(lbl_value, buf);
        }
        return;
    }

    String display_text;
    if (input_buffer.length() > 0) {
        display_text = input_buffer;
    } else {
        display_text = "---";
    }

    // Add suffix: esplicito se passato, altrimenti dedotto dal range
    if (unit_suffix) {
        display_text += " ";
        display_text += unit_suffix;
    } else if (max_value <= 100 && max_value != 99) {
        display_text += " %";
    } else if (max_value == 99) {
        display_text += " min";
    } else {
        display_text += " °C";
    }

    lv_label_set_text(lbl_value, display_text.c_str());
}

// ============================================
// Keypad event callback
// ============================================
static void keypad_event_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    const char* txt = lv_btnmatrix_get_btn_text(obj, id);

    if (!txt) return;

    buzzer_beep();

    if (strcmp(txt, "C") == 0) {
        if (input_buffer.length() == 0) {
            // C a campo vuoto: annulla e torna indietro senza modifiche
            Serial.println("Keypad: Annulla");
            ui_keypad_hide();
            return;
        }
        // Clear
        input_buffer = "";
        s_preset_sum = false;
        Serial.println("Keypad: Clear");
    }
    else if (strcmp(txt, "OK") == 0 && s_time_mode) {
        if (input_buffer.length() == 0) {
            ui_keypad_hide();
            return;
        }
        int n = input_buffer.toInt();
        if (n % 100 > 59) {
            buzzer_error();
            input_buffer = "";
            lv_label_set_text(lbl_value, s_clock_mode ? "MIN 0-59" : "SEC 0-59");
            lv_obj_set_style_text_color(lbl_value, COLOR_RED, 0);
            return;
        }
        if (s_clock_mode && n / 100 > 23) {
            buzzer_error();
            input_buffer = "";
            lv_label_set_text(lbl_value, "ORE 0-23");
            lv_obj_set_style_text_color(lbl_value, COLOR_RED, 0);
            return;
        }
        commit_time((n / 100) * 60 + n % 100);
        return;
    }
    else if (strcmp(txt, "OK") == 0) {
        // Confirm
        if (input_buffer.length() > 0 && edit_target) {
            int val = input_buffer.toInt();
            if (val < min_value || val > max_value) {
                // Fuori range: prima il tastierino si chiudeva come se il
                // valore fosse stato accettato. Resta aperto e mostra i limiti
                buzzer_error();
                input_buffer = "";
                char buf[24];
                snprintf(buf, sizeof(buf), "%d - %d", min_value, max_value);
                lv_label_set_text(lbl_value, buf);
                lv_obj_set_style_text_color(lbl_value, COLOR_RED, 0);
                Serial.printf("Keypad: %d fuori range %d-%d\n", val, min_value, max_value);
                return;
            }
            *edit_target = val;
            buzzer_confirm();  // Double beep on successful confirm
            Serial.printf("Keypad: Set value to %d\n", val);

            // Se siamo in programma manuale (P0), salva automaticamente
            if (g_state.program_number == PROGRAM_MANUAL) {
                programs_save_manual_params();
            }
        }
        ui_keypad_hide();
        return;
    }
    else {
        // Digit input
        int max_digits;
        if (s_time_mode) {
            max_digits = 4;  // fino a 99:59
        } else if (max_value >= 100) {
            max_digits = 3;
        } else {
            max_digits = 2;
        }

        if (s_preset_sum) {
            // Dopo i preset una cifra inizia un tempo nuovo, non si accoda
            input_buffer = "";
            s_preset_sum = false;
        }
        if ((int)input_buffer.length() < max_digits) {
            input_buffer += txt;
            Serial.printf("Keypad: Input '%s'\n", input_buffer.c_str());
        }
    }

    update_display();
}
