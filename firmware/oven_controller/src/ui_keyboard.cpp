/**
 * Alphanumeric Keyboard Implementation
 * Layout QWERTY per inserire nomi programmi
 */

#include "ui_keyboard.h"
#include "ui_theme.h"
#include "ui_programs.h"
#include "buzzer.h"
#include <Arduino.h>

// ============================================
// Static variables
// ============================================
static lv_obj_t* screen_keyboard = nullptr;
static lv_obj_t* lbl_title = nullptr;
static lv_obj_t* lbl_input = nullptr;
static lv_obj_t* lbl_counter = nullptr;
static lv_obj_t* btnm_keys = nullptr;

static char* s_target = nullptr;
static uint8_t s_max_len = 16;
static String s_input = "";
static bool s_keyboard_visible = false;

// Layout tastiera QWERTY
static const char* kb_map[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "\n",
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "\n",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", LV_SYMBOL_BACKSPACE, "\n",
    "Z", "X", "C", "V", "B", "N", "M", " ", "OK", ""
};

// ============================================
// Forward declarations
// ============================================
static void keyboard_event_cb(lv_event_t* e);
static void update_display();

// ============================================
// Show keyboard
// ============================================
void ui_keyboard_show(char* target, const char* title, uint8_t max_len) {
    s_target = target;
    s_max_len = max_len;
    s_input = target ? target : "";  // Carica valore attuale
    s_keyboard_visible = true;

    if (!screen_keyboard) {
        screen_keyboard = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_keyboard, COLOR_BG, 0);
        lv_obj_clear_flag(screen_keyboard, LV_OBJ_FLAG_SCROLLABLE);

        // Title
        lbl_title = lv_label_create(screen_keyboard);
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(lbl_title, COLOR_WHITE, 0);
        lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 15);

        // Input display container
        lv_obj_t* input_cont = lv_obj_create(screen_keyboard);
        lv_obj_set_size(input_cont, SCREEN_WIDTH - 20, 50);
        lv_obj_align(input_cont, LV_ALIGN_TOP_MID, 0, 50);
        lv_obj_set_style_bg_color(input_cont, COLOR_PANEL_BG, 0);
        lv_obj_set_style_radius(input_cont, 8, 0);
        lv_obj_set_style_border_width(input_cont, 1, 0);
        lv_obj_set_style_border_color(input_cont, COLOR_ORANGE, 0);
        lv_obj_clear_flag(input_cont, LV_OBJ_FLAG_SCROLLABLE);

        // Input text
        lbl_input = lv_label_create(input_cont);
        lv_obj_set_style_text_font(lbl_input, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_input, COLOR_YELLOW, 0);
        lv_obj_align(lbl_input, LV_ALIGN_LEFT_MID, 10, 0);

        // Character counter
        lbl_counter = lv_label_create(input_cont);
        lv_obj_set_style_text_font(lbl_counter, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lbl_counter, COLOR_WHITE, 0);
        lv_obj_align(lbl_counter, LV_ALIGN_RIGHT_MID, -10, 0);

        // Keyboard button matrix
        btnm_keys = lv_btnmatrix_create(screen_keyboard);
        lv_btnmatrix_set_map(btnm_keys, kb_map);
        lv_obj_set_size(btnm_keys, SCREEN_WIDTH - 10, 320);
        lv_obj_align(btnm_keys, LV_ALIGN_BOTTOM_MID, 0, -10);

        // Style
        lv_obj_set_style_bg_color(btnm_keys, COLOR_BG, 0);
        lv_obj_set_style_border_width(btnm_keys, 0, 0);
        lv_obj_set_style_pad_all(btnm_keys, 3, 0);

        // Button style
        lv_obj_set_style_bg_color(btnm_keys, COLOR_BUTTON_BG, LV_PART_ITEMS);
        lv_obj_set_style_bg_opa(btnm_keys, LV_OPA_COVER, LV_PART_ITEMS);
        lv_obj_set_style_radius(btnm_keys, 6, LV_PART_ITEMS);
        lv_obj_set_style_border_width(btnm_keys, 1, LV_PART_ITEMS);
        lv_obj_set_style_border_color(btnm_keys, COLOR_DARK_GRAY, LV_PART_ITEMS);
        lv_obj_set_style_text_color(btnm_keys, COLOR_WHITE, LV_PART_ITEMS);
        lv_obj_set_style_text_font(btnm_keys, &lv_font_montserrat_18, LV_PART_ITEMS);

        // Pressed style
        lv_obj_set_style_bg_color(btnm_keys, COLOR_GRAY, LV_PART_ITEMS | LV_STATE_PRESSED);

        lv_obj_add_event_cb(btnm_keys, keyboard_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    }

    // Update title
    lv_label_set_text(lbl_title, title);

    // Update display
    update_display();

    // Load screen
    lv_scr_load(screen_keyboard);
    Serial.printf("Keyboard shown: %s\n", title);
}

// ============================================
// Hide keyboard
// ============================================
void ui_keyboard_hide() {
    s_keyboard_visible = false;
    s_target = nullptr;

    // Torna alla schermata programmi (o edit)
    extern void ui_program_edit_refresh();
    ui_program_edit_refresh();

    Serial.println("Keyboard hidden");
}

// ============================================
// Check visibility
// ============================================
bool ui_keyboard_is_visible() {
    return s_keyboard_visible;
}

// ============================================
// Update display
// ============================================
static void update_display() {
    if (!lbl_input || !lbl_counter) return;

    // Input con cursore
    String display = s_input + "_";
    lv_label_set_text(lbl_input, display.c_str());

    // Counter
    char counter[16];
    snprintf(counter, sizeof(counter), "%d/%d", s_input.length(), s_max_len);
    lv_label_set_text(lbl_counter, counter);
}

// ============================================
// Keyboard event callback
// ============================================
static void keyboard_event_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    uint32_t id = lv_btnmatrix_get_selected_btn(obj);
    const char* txt = lv_btnmatrix_get_btn_text(obj, id);

    if (!txt) return;

    buzzer_beep();

    if (strcmp(txt, LV_SYMBOL_BACKSPACE) == 0) {
        // Backspace
        if (s_input.length() > 0) {
            s_input.remove(s_input.length() - 1);
        }
    }
    else if (strcmp(txt, "OK") == 0) {
        // Conferma
        if (s_target) {
            strncpy(s_target, s_input.c_str(), s_max_len);
            s_target[s_max_len] = '\0';
            buzzer_confirm();
            Serial.printf("Keyboard: Set '%s'\n", s_target);
        }
        ui_keyboard_hide();
        return;
    }
    else if (strcmp(txt, " ") == 0) {
        // Spazio
        if ((int)s_input.length() < s_max_len) {
            s_input += " ";
        }
    }
    else {
        // Carattere normale
        if ((int)s_input.length() < s_max_len) {
            s_input += txt;
        }
    }

    update_display();
}
