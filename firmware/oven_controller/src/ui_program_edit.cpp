/**
 * Program Edit Screen Implementation - Effeuno Table Style
 * Schermata modifica programmi con layout a tabella identico all'originale
 */

#include "ui_program_edit.h"
#include "ui_programs.h"
#include "ui_keyboard.h"
#include "ui_keypad.h"
#include "ui_theme.h"
#include "programs_storage.h"
#include "oven_state.h"
#include "buzzer.h"

// ============================================
// Layout constants - Effeuno table style
// ============================================
#define EDIT_HEADER_HEIGHT      45
#define EDIT_ROW_HEIGHT         36
#define EDIT_LABEL_WIDTH        150
#define EDIT_PHASE_COL_WIDTH    56
#define EDIT_VALUE_WIDTH        70

// Colors for table cells (matching Effeuno image)
#define COLOR_CELL_BG           lv_color_hex(0x1A1A1A)  // Dark gray cell bg
#define COLOR_CELL_VALUE        lv_color_hex(0x2A2A2A)  // Slightly lighter for value cells
#define COLOR_CELL_BORDER       lv_color_hex(0x404040)  // Cell border

// ============================================
// Static UI elements
// ============================================
static lv_obj_t* screen_edit = nullptr;
static lv_obj_t* lbl_prog_title = nullptr;

// Info section labels
static lv_obj_t* lbl_prog_num = nullptr;
static lv_obj_t* lbl_description = nullptr;
static lv_obj_t* lbl_num_phases = nullptr;

// Phase table value cells
static lv_obj_t* phase_headers[MAX_PHASES] = {nullptr, nullptr, nullptr};
static lv_obj_t* lbl_phase_temp_cielo[MAX_PHASES];
static lv_obj_t* lbl_phase_temp_platea[MAX_PHASES];
static lv_obj_t* lbl_phase_time[MAX_PHASES];
static lv_obj_t* lbl_phase_pot_cielo[MAX_PHASES];
static lv_obj_t* lbl_phase_pot_platea[MAX_PHASES];

// Global params labels
static lv_obj_t* lbl_boost_time = nullptr;

static bool edit_visible = false;
static uint8_t edit_program = 0;
static Program edit_copy;  // Working copy of program being edited

// Enum for tracking which uint8_t field is being edited
enum EditField {
    EDIT_NONE = 0,
    EDIT_BOOST_TIME
};
static EditField current_edit_field = EDIT_NONE;

// Temporary variable for keypad editing (uint8_t fields)
static int temp_edit_value = 0;

// ============================================
// Forward declarations
// ============================================
static void on_back_click(lv_event_t* e);
static void on_save_click(lv_event_t* e);
static void on_description_click(lv_event_t* e);
static void on_num_phases_click(lv_event_t* e);
static void on_phase_temp_cielo_click(lv_event_t* e);
static void on_phase_temp_platea_click(lv_event_t* e);
static void on_phase_time_click(lv_event_t* e);
static void on_phase_pot_cielo_click(lv_event_t* e);
static void on_phase_pot_platea_click(lv_event_t* e);
static void on_boost_click(lv_event_t* e);
static void update_display();
static void update_phase_visibility();

// Helper to create a table cell
static lv_obj_t* create_cell(lv_obj_t* parent, int x, int y, int w, int h,
                              lv_color_t bg_color, bool clickable);
static lv_obj_t* create_label_cell(lv_obj_t* parent, int x, int y, int w, int h,
                                    const char* text);
static lv_obj_t* create_value_cell(lv_obj_t* parent, int x, int y, int w, int h,
                                    lv_event_cb_t cb, void* user_data);

// ============================================
// Helper: Create a generic cell
// ============================================
static lv_obj_t* create_cell(lv_obj_t* parent, int x, int y, int w, int h,
                              lv_color_t bg_color, bool clickable) {
    lv_obj_t* cell = lv_obj_create(parent);
    lv_obj_set_size(cell, w, h);
    lv_obj_set_pos(cell, x, y);
    lv_obj_set_style_bg_color(cell, bg_color, 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(cell, 0, 0);
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, COLOR_CELL_BORDER, 0);
    lv_obj_set_style_pad_all(cell, 2, 0);
    lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);

    if (clickable) {
        lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(cell, lv_color_hex(0x404040), LV_STATE_PRESSED);
    }

    return cell;
}

// ============================================
// Helper: Create a label cell (left side)
// ============================================
static lv_obj_t* create_label_cell(lv_obj_t* parent, int x, int y, int w, int h,
                                    const char* text) {
    lv_obj_t* cell = create_cell(parent, x, y, w, h, COLOR_CELL_BG, false);

    lv_obj_t* lbl = lv_label_create(cell);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
    lv_label_set_text(lbl, text);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 2, 0);

    return cell;
}

// ============================================
// Helper: Create a value cell (clickable)
// ============================================
static lv_obj_t* create_value_cell(lv_obj_t* parent, int x, int y, int w, int h,
                                    lv_event_cb_t cb, void* user_data) {
    lv_obj_t* cell = create_cell(parent, x, y, w, h, COLOR_CELL_VALUE, cb != nullptr);

    if (cb) {
        lv_obj_add_event_cb(cell, cb, LV_EVENT_CLICKED, user_data);
    }

    // Create label inside for value display
    lv_obj_t* lbl = lv_label_create(cell);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
    lv_label_set_text(lbl, "");
    lv_obj_center(lbl);

    return lbl;  // Return the label, not the cell
}

// ============================================
// Show program edit screen
// ============================================
void ui_program_edit_show(uint8_t program_index) {
    if (program_index >= MAX_PROGRAMS) {
        buzzer_error();
        return;
    }

    edit_visible = true;
    edit_program = program_index;

    // Make a working copy of the program
    memcpy(&edit_copy, &g_programs[program_index], sizeof(Program));

    // Create screen if not exists
    if (!screen_edit) {
        screen_edit = lv_obj_create(NULL);
        lv_obj_set_style_bg_color(screen_edit, COLOR_BG, 0);
        lv_obj_clear_flag(screen_edit, LV_OBJ_FLAG_SCROLLABLE);

        int y = 0;

        // ==========================================
        // Header
        // ==========================================
        lv_obj_t* header = lv_obj_create(screen_edit);
        lv_obj_set_size(header, SCREEN_WIDTH, EDIT_HEADER_HEIGHT);
        lv_obj_set_pos(header, 0, 0);
        lv_obj_set_style_bg_color(header, COLOR_HEADER_BG, 0);
        lv_obj_set_style_radius(header, 0, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        // Back button
        lv_obj_t* btn_back = lv_btn_create(header);
        lv_obj_set_size(btn_back, 45, 36);
        lv_obj_set_pos(btn_back, 5, 4);
        lv_obj_set_style_bg_color(btn_back, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_radius(btn_back, 6, 0);
        lv_obj_add_event_cb(btn_back, on_back_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_back = lv_label_create(btn_back);
        lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_back, COLOR_WHITE, 0);
        lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
        lv_obj_center(lbl_back);

        // Program title
        lbl_prog_title = lv_label_create(header);
        lv_obj_set_style_text_font(lbl_prog_title, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_prog_title, COLOR_WHITE, 0);
        lv_obj_align(lbl_prog_title, LV_ALIGN_CENTER, 0, 0);

        // Save button
        lv_obj_t* btn_save = lv_btn_create(header);
        lv_obj_set_size(btn_save, 60, 36);
        lv_obj_set_pos(btn_save, SCREEN_WIDTH - 68, 4);
        lv_obj_set_style_bg_color(btn_save, COLOR_GREEN, 0);
        lv_obj_set_style_radius(btn_save, 6, 0);
        lv_obj_add_event_cb(btn_save, on_save_click, LV_EVENT_CLICKED, NULL);

        lv_obj_t* lbl_save = lv_label_create(btn_save);
        lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_save, COLOR_BLACK, 0);
        lv_label_set_text(lbl_save, "SALVA");
        lv_obj_center(lbl_save);

        y = EDIT_HEADER_HEIGHT;

        // ==========================================
        // Info Section (Programma, Descrizione, Numero fasi)
        // ==========================================

        // Row: Programma
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH + 10, EDIT_ROW_HEIGHT, "Programma");
        lv_obj_t* cell_prog = create_cell(screen_edit, EDIT_LABEL_WIDTH + 10, y,
                                          SCREEN_WIDTH - EDIT_LABEL_WIDTH - 10, EDIT_ROW_HEIGHT,
                                          COLOR_CELL_VALUE, false);
        lbl_prog_num = lv_label_create(cell_prog);
        lv_obj_set_style_text_font(lbl_prog_num, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_prog_num, COLOR_WHITE, 0);
        lv_obj_center(lbl_prog_num);
        y += EDIT_ROW_HEIGHT;

        // Row: Descrizione
        lv_obj_t* desc_label = create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH + 10, EDIT_ROW_HEIGHT, "Descrizione");
        lv_obj_t* desc_cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + 10, y,
                                          SCREEN_WIDTH - EDIT_LABEL_WIDTH - 10, EDIT_ROW_HEIGHT,
                                          COLOR_CELL_VALUE, true);
        lv_obj_add_event_cb(desc_cell, on_description_click, LV_EVENT_CLICKED, NULL);
        lbl_description = lv_label_create(desc_cell);
        lv_obj_set_style_text_font(lbl_description, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_description, COLOR_WHITE, 0);
        lv_obj_center(lbl_description);
        y += EDIT_ROW_HEIGHT;

        // Row: Numero fasi
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH + 10, EDIT_ROW_HEIGHT, "Numero fasi");
        lv_obj_t* phases_cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + 10, y,
                                             SCREEN_WIDTH - EDIT_LABEL_WIDTH - 10, EDIT_ROW_HEIGHT,
                                             COLOR_CELL_VALUE, true);
        lv_obj_add_event_cb(phases_cell, on_num_phases_click, LV_EVENT_CLICKED, NULL);
        lbl_num_phases = lv_label_create(phases_cell);
        lv_obj_set_style_text_font(lbl_num_phases, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_num_phases, COLOR_WHITE, 0);
        lv_obj_center(lbl_num_phases);
        y += EDIT_ROW_HEIGHT;

        // Small spacer
        y += 4;

        // ==========================================
        // Phase Table Header
        // ==========================================
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH, EDIT_ROW_HEIGHT, "");
        for (int p = 0; p < MAX_PHASES; p++) {
            lv_obj_t* hdr_cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + p * EDIT_PHASE_COL_WIDTH, y,
                                             EDIT_PHASE_COL_WIDTH, EDIT_ROW_HEIGHT, COLOR_CELL_BG, false);
            phase_headers[p] = lv_label_create(hdr_cell);
            lv_obj_set_style_text_font(phase_headers[p], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(phase_headers[p], COLOR_ORANGE, 0);
            char hdr_txt[12];
            snprintf(hdr_txt, sizeof(hdr_txt), "Fase %d", p + 1);
            lv_label_set_text(phase_headers[p], hdr_txt);
            lv_obj_center(phase_headers[p]);
        }
        y += EDIT_ROW_HEIGHT;

        // ==========================================
        // Phase Table Data Rows
        // ==========================================

        // Row: Temper.Cielo (°C)
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH, EDIT_ROW_HEIGHT, "Temper.Cielo (\xC2\xB0" "C)");
        for (int p = 0; p < MAX_PHASES; p++) {
            lv_obj_t* cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + p * EDIT_PHASE_COL_WIDTH, y,
                                         EDIT_PHASE_COL_WIDTH, EDIT_ROW_HEIGHT, COLOR_CELL_VALUE, true);
            lv_obj_add_event_cb(cell, on_phase_temp_cielo_click, LV_EVENT_CLICKED, (void*)(intptr_t)p);
            lbl_phase_temp_cielo[p] = lv_label_create(cell);
            lv_obj_set_style_text_font(lbl_phase_temp_cielo[p], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl_phase_temp_cielo[p], COLOR_WHITE, 0);
            lv_obj_center(lbl_phase_temp_cielo[p]);
        }
        y += EDIT_ROW_HEIGHT;

        // Row: Temper.Platea (°C)
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH, EDIT_ROW_HEIGHT, "Temper.Platea (\xC2\xB0" "C)");
        for (int p = 0; p < MAX_PHASES; p++) {
            lv_obj_t* cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + p * EDIT_PHASE_COL_WIDTH, y,
                                         EDIT_PHASE_COL_WIDTH, EDIT_ROW_HEIGHT, COLOR_CELL_VALUE, true);
            lv_obj_add_event_cb(cell, on_phase_temp_platea_click, LV_EVENT_CLICKED, (void*)(intptr_t)p);
            lbl_phase_temp_platea[p] = lv_label_create(cell);
            lv_obj_set_style_text_font(lbl_phase_temp_platea[p], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl_phase_temp_platea[p], COLOR_WHITE, 0);
            lv_obj_center(lbl_phase_temp_platea[p]);
        }
        y += EDIT_ROW_HEIGHT;

        // Row: T.cottura (mm:ss)
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH, EDIT_ROW_HEIGHT, "T.cottura (mm:ss)");
        for (int p = 0; p < MAX_PHASES; p++) {
            lv_obj_t* cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + p * EDIT_PHASE_COL_WIDTH, y,
                                         EDIT_PHASE_COL_WIDTH, EDIT_ROW_HEIGHT, COLOR_CELL_VALUE, true);
            lv_obj_add_event_cb(cell, on_phase_time_click, LV_EVENT_CLICKED, (void*)(intptr_t)p);
            lbl_phase_time[p] = lv_label_create(cell);
            lv_obj_set_style_text_font(lbl_phase_time[p], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl_phase_time[p], COLOR_WHITE, 0);
            lv_obj_center(lbl_phase_time[p]);
        }
        y += EDIT_ROW_HEIGHT;

        // Row: Pot.Cielo (%)
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH, EDIT_ROW_HEIGHT, "Pot.Cielo (%)");
        for (int p = 0; p < MAX_PHASES; p++) {
            lv_obj_t* cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + p * EDIT_PHASE_COL_WIDTH, y,
                                         EDIT_PHASE_COL_WIDTH, EDIT_ROW_HEIGHT, COLOR_CELL_VALUE, true);
            lv_obj_add_event_cb(cell, on_phase_pot_cielo_click, LV_EVENT_CLICKED, (void*)(intptr_t)p);
            lbl_phase_pot_cielo[p] = lv_label_create(cell);
            lv_obj_set_style_text_font(lbl_phase_pot_cielo[p], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl_phase_pot_cielo[p], COLOR_WHITE, 0);
            lv_obj_center(lbl_phase_pot_cielo[p]);
        }
        y += EDIT_ROW_HEIGHT;

        // Row: Pot.Platea (%)
        create_label_cell(screen_edit, 0, y, EDIT_LABEL_WIDTH, EDIT_ROW_HEIGHT, "Pot.Platea (%)");
        for (int p = 0; p < MAX_PHASES; p++) {
            lv_obj_t* cell = create_cell(screen_edit, EDIT_LABEL_WIDTH + p * EDIT_PHASE_COL_WIDTH, y,
                                         EDIT_PHASE_COL_WIDTH, EDIT_ROW_HEIGHT, COLOR_CELL_VALUE, true);
            lv_obj_add_event_cb(cell, on_phase_pot_platea_click, LV_EVENT_CLICKED, (void*)(intptr_t)p);
            lbl_phase_pot_platea[p] = lv_label_create(cell);
            lv_obj_set_style_text_font(lbl_phase_pot_platea[p], &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl_phase_pot_platea[p], COLOR_WHITE, 0);
            lv_obj_center(lbl_phase_pot_platea[p]);
        }
        y += EDIT_ROW_HEIGHT;

        // Small spacer
        y += 4;

        // ==========================================
        // Global Parameters Section
        // ==========================================

        int global_label_w = 235;
        int global_value_w = SCREEN_WIDTH - global_label_w;

        // Row: Tempo Boost (s)
        create_label_cell(screen_edit, 0, y, global_label_w, EDIT_ROW_HEIGHT, "Tempo Boost (s)");
        lv_obj_t* cell_boost = create_cell(screen_edit, global_label_w, y, global_value_w, EDIT_ROW_HEIGHT,
                                           COLOR_CELL_VALUE, true);
        lv_obj_add_event_cb(cell_boost, on_boost_click, LV_EVENT_CLICKED, NULL);
        lbl_boost_time = lv_label_create(cell_boost);
        lv_obj_set_style_text_font(lbl_boost_time, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_boost_time, COLOR_WHITE, 0);
        lv_obj_center(lbl_boost_time);
    }

    // Update display with current program values
    update_display();
    update_phase_visibility();

    // Load screen
    lv_scr_load(screen_edit);
    Serial.printf("Program edit shown: P%d\n", program_index);
}

// ============================================
// Hide program edit screen
// ============================================
void ui_program_edit_hide() {
    edit_visible = false;

    // IMPORTANTE: Prima caricare la nuova schermata, poi eliminare la vecchia
    ui_programs_show_manage();  // Torna a gestione programmi

    // Ora elimina la schermata edit (non è più attiva)
    if (screen_edit) {
        lv_obj_del(screen_edit);
        screen_edit = nullptr;
        lbl_prog_title = nullptr;
        lbl_prog_num = nullptr;
        lbl_description = nullptr;
        lbl_num_phases = nullptr;
        for (int p = 0; p < MAX_PHASES; p++) {
            phase_headers[p] = nullptr;
            lbl_phase_temp_cielo[p] = nullptr;
            lbl_phase_temp_platea[p] = nullptr;
            lbl_phase_time[p] = nullptr;
            lbl_phase_pot_cielo[p] = nullptr;
            lbl_phase_pot_platea[p] = nullptr;
        }
        lbl_boost_time = nullptr;
    }
}

// ============================================
// Check visibility
// ============================================
bool ui_program_edit_is_visible() {
    return edit_visible;
}

// ============================================
// Refresh display (called after keyboard/keypad)
// ============================================
void ui_program_edit_refresh() {
    if (edit_visible && screen_edit) {
        // Copy back temp value to appropriate uint8_t field
        switch (current_edit_field) {
            case EDIT_BOOST_TIME:
                edit_copy.boost_time = (uint8_t)temp_edit_value;
                break;
            default:
                break;
        }
        current_edit_field = EDIT_NONE;

        update_display();
        update_phase_visibility();
        lv_scr_load(screen_edit);
    }
}

// ============================================
// Update display with current values
// ============================================
static void update_display() {
    if (!screen_edit) return;

    char buf[24];

    // Program title in header
    snprintf(buf, sizeof(buf), "Modifica P%d", edit_program);
    lv_label_set_text(lbl_prog_title, buf);

    // Program number
    snprintf(buf, sizeof(buf), "P%d", edit_program);
    lv_label_set_text(lbl_prog_num, buf);

    // Description (name)
    lv_label_set_text(lbl_description, edit_copy.name);

    // Number of phases
    snprintf(buf, sizeof(buf), "%d", edit_copy.num_phases);
    lv_label_set_text(lbl_num_phases, buf);

    // Phase parameters
    for (int p = 0; p < MAX_PHASES; p++) {
        // Temperature Cielo
        snprintf(buf, sizeof(buf), "%d", edit_copy.phases[p].temp_cielo);
        lv_label_set_text(lbl_phase_temp_cielo[p], buf);

        // Temperature Platea
        snprintf(buf, sizeof(buf), "%d", edit_copy.phases[p].temp_platea);
        lv_label_set_text(lbl_phase_temp_platea[p], buf);

        // Duration as mm:ss
        int mins = edit_copy.phases[p].duration;
        if (mins == 0) {
            lv_label_set_text(lbl_phase_time[p], "--:--");
        } else {
            snprintf(buf, sizeof(buf), "%02d:00", mins);
            lv_label_set_text(lbl_phase_time[p], buf);
        }

        // Power Cielo
        snprintf(buf, sizeof(buf), "%d", edit_copy.phases[p].power_cielo);
        lv_label_set_text(lbl_phase_pot_cielo[p], buf);

        // Power Platea
        snprintf(buf, sizeof(buf), "%d", edit_copy.phases[p].power_platea);
        lv_label_set_text(lbl_phase_pot_platea[p], buf);
    }

    // Global parameters
    snprintf(buf, sizeof(buf), "%d", edit_copy.boost_time);
    lv_label_set_text(lbl_boost_time, buf);
}

// ============================================
// Update phase column visibility
// ============================================
static void update_phase_visibility() {
    for (int p = 0; p < MAX_PHASES; p++) {
        bool visible = (p < edit_copy.num_phases);

        // Get parent cells from the labels
        if (lbl_phase_temp_cielo[p]) {
            lv_obj_t* parent = lv_obj_get_parent(lbl_phase_temp_cielo[p]);
            if (visible) {
                lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (lbl_phase_temp_platea[p]) {
            lv_obj_t* parent = lv_obj_get_parent(lbl_phase_temp_platea[p]);
            if (visible) {
                lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (lbl_phase_time[p]) {
            lv_obj_t* parent = lv_obj_get_parent(lbl_phase_time[p]);
            if (visible) {
                lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (lbl_phase_pot_cielo[p]) {
            lv_obj_t* parent = lv_obj_get_parent(lbl_phase_pot_cielo[p]);
            if (visible) {
                lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
            }
        }
        if (lbl_phase_pot_platea[p]) {
            lv_obj_t* parent = lv_obj_get_parent(lbl_phase_pot_platea[p]);
            if (visible) {
                lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
            }
        }
        // Phase header
        if (phase_headers[p]) {
            lv_obj_t* parent = lv_obj_get_parent(phase_headers[p]);
            if (visible) {
                lv_obj_clear_flag(parent, LV_OBJ_FLAG_HIDDEN);
            } else {
                lv_obj_add_flag(parent, LV_OBJ_FLAG_HIDDEN);
            }
        }
    }
}

// ============================================
// Event callbacks
// ============================================
static void on_back_click(lv_event_t* e) {
    buzzer_beep();
    ui_program_edit_hide();
}

static void on_save_click(lv_event_t* e) {
    buzzer_confirm();

    // Copy edited values back to g_programs
    memcpy(&g_programs[edit_program], &edit_copy, sizeof(Program));

    // Save to NVS
    programs_save(edit_program);

    Serial.printf("Program P%d saved\n", edit_program);

    ui_program_edit_hide();
}

static void on_description_click(lv_event_t* e) {
    buzzer_beep();
    ui_keyboard_show(edit_copy.name, "Nome programma", MAX_PROGRAM_NAME - 1);
}

static void on_num_phases_click(lv_event_t* e) {
    buzzer_beep();
    // Cycle through 1-2-3
    edit_copy.num_phases++;
    if (edit_copy.num_phases > MAX_PHASES) {
        edit_copy.num_phases = 1;
    }
    update_display();
    update_phase_visibility();
}

static void on_phase_temp_cielo_click(lv_event_t* e) {
    buzzer_beep();
    current_edit_field = EDIT_NONE;
    int phase = (int)(intptr_t)lv_event_get_user_data(e);
    ui_keypad_show(&edit_copy.phases[phase].temp_cielo, "Temp. Cielo", 50, 500);
}

static void on_phase_temp_platea_click(lv_event_t* e) {
    buzzer_beep();
    current_edit_field = EDIT_NONE;
    int phase = (int)(intptr_t)lv_event_get_user_data(e);
    ui_keypad_show(&edit_copy.phases[phase].temp_platea, "Temp. Platea", 50, 500);
}

static void on_phase_time_click(lv_event_t* e) {
    buzzer_beep();
    current_edit_field = EDIT_NONE;
    int phase = (int)(intptr_t)lv_event_get_user_data(e);
    ui_keypad_show(&edit_copy.phases[phase].duration, "Durata (min)", 0, 99);
}

static void on_phase_pot_cielo_click(lv_event_t* e) {
    buzzer_beep();
    current_edit_field = EDIT_NONE;
    int phase = (int)(intptr_t)lv_event_get_user_data(e);
    ui_keypad_show(&edit_copy.phases[phase].power_cielo, "Potenza Cielo %", 0, 100);
}

static void on_phase_pot_platea_click(lv_event_t* e) {
    buzzer_beep();
    current_edit_field = EDIT_NONE;
    int phase = (int)(intptr_t)lv_event_get_user_data(e);
    ui_keypad_show(&edit_copy.phases[phase].power_platea, "Potenza Platea %", 0, 100);
}

static void on_boost_click(lv_event_t* e) {
    buzzer_beep();
    current_edit_field = EDIT_BOOST_TIME;
    temp_edit_value = edit_copy.boost_time;
    ui_keypad_show(&temp_edit_value, "Tempo Boost (sec)", 0, 180, "s");
}
