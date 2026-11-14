/**
 * Programs Selection Screen Implementation
 *
 * Un solo elenco P0-P19 che si scorre col dito (prima: pagine da 7 con
 * frecce). Ogni riga su due righe di testo: in alto numero e nome intero,
 * sotto temperature cielo/platea, tempo e fasi. Prima nome e dati stavano
 * sulla stessa riga e i nomi lunghi finivano sopra i dati.
 * Il programma attivo ha il bordo arancio e l'elenco si apre su di lui.
 */

#include "ui_programs.h"
#include "ui_theme.h"
#include "ui_main.h"
#include "ui_auto.h"
#include "ui_settings.h"
#include "ui_program_edit.h"
#include "programs_storage.h"
#include "oven_state.h"
#include "buzzer.h"

// ============================================
// Costanti layout
// ============================================
#define PROGS_HEADER_HEIGHT     50
#define PROGS_HINT_HEIGHT       30
#define PROGS_LIST_Y            (PROGS_HEADER_HEIGHT + PROGS_HINT_HEIGHT)
#define PROGS_ROW_H             58
#define PROGS_ROW_GAP           6

// ============================================
// Static UI elements
// ============================================
static lv_obj_t* screen_programs = nullptr;
static lv_obj_t* lbl_title = nullptr;
static lv_obj_t* lbl_hint = nullptr;
static lv_obj_t* list_container = nullptr;
static lv_obj_t* items[MAX_PROGRAMS];
static lv_obj_t* badges[MAX_PROGRAMS];
static lv_obj_t* names[MAX_PROGRAMS];
static lv_obj_t* descs[MAX_PROGRAMS];

static bool programs_visible = false;
static bool manage_mode = false;  // false=select, true=manage/edit

// ============================================
// Forward declarations
// ============================================
static void on_back_click(lv_event_t* e);
static void on_item_click(lv_event_t* e);
static void create_screen();
static void update_list();
static void apply_mode();

// ============================================
// Show programs screen
// ============================================
void ui_programs_show() {
    programs_visible = true;
    manage_mode = false;  // Modalita' selezione

    if (!screen_programs) create_screen();
    apply_mode();
    update_list();

    lv_scr_load(screen_programs);
    Serial.println("Programs screen shown");
}

// ============================================
// Show programs screen in manage mode
// ============================================
void ui_programs_show_manage() {
    programs_visible = true;
    manage_mode = true;  // Modalita' gestione

    if (!screen_programs) create_screen();
    apply_mode();
    update_list();

    lv_scr_load(screen_programs);
    Serial.println("Programs manage screen shown");
}

static void create_screen() {
    screen_programs = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_programs, COLOR_BG, 0);
    lv_obj_clear_flag(screen_programs, LV_OBJ_FLAG_SCROLLABLE);

    // --- Header: indietro + titolo (prima c'era la data, gia' nella home) ---
    lv_obj_t* header = lv_obj_create(screen_programs);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, SCREEN_WIDTH, PROGS_HEADER_HEIGHT);
    lv_obj_set_style_bg_color(header, COLOR_HEADER_BG, 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 56, 40);
    lv_obj_set_pos(btn_back, 6, 5);
    lv_obj_set_style_bg_color(btn_back, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_border_color(btn_back, COLOR_DARK_GRAY, 0);
    lv_obj_set_style_border_width(btn_back, 1, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_radius(btn_back, 8, 0);
    lv_obj_add_event_cb(btn_back, on_back_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_back = lv_label_create(btn_back);
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_back, COLOR_WHITE, 0);
    lv_label_set_text(lbl_back, LV_SYMBOL_LEFT);
    lv_obj_center(lbl_back);

    lbl_title = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, COLOR_WHITE, 0);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 12, 0);

    // --- Cosa succede toccando una riga ---
    lbl_hint = lv_label_create(screen_programs);
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_hint, COLOR_WHITE, 0);
    lv_obj_align(lbl_hint, LV_ALIGN_TOP_MID, 0, PROGS_HEADER_HEIGHT + 7);

    // --- Elenco scorrevole ---
    list_container = lv_obj_create(screen_programs);
    lv_obj_remove_style_all(list_container);
    lv_obj_set_size(list_container, SCREEN_WIDTH, SCREEN_HEIGHT - PROGS_LIST_Y);
    lv_obj_set_pos(list_container, 0, PROGS_LIST_Y);
    lv_obj_set_style_pad_hor(list_container, 10, 0);
    lv_obj_set_style_pad_bottom(list_container, 10, 0);
    lv_obj_set_flex_flow(list_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(list_container, PROGS_ROW_GAP, 0);
    lv_obj_set_scroll_dir(list_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list_container, LV_SCROLLBAR_MODE_ACTIVE);

    for (int i = 0; i < MAX_PROGRAMS; i++) {
        lv_obj_t* row = lv_obj_create(list_container);
        lv_obj_remove_style_all(row);
        lv_obj_set_size(row, lv_pct(100), PROGS_ROW_H);
        lv_obj_set_style_bg_color(row, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(row, COLOR_DARK_GRAY, LV_STATE_PRESSED);
        lv_obj_set_style_border_color(row, COLOR_DARK_GRAY, 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 10, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        // Il trascinamento sulla riga scorre l'elenco
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLL_CHAIN);
        lv_obj_add_event_cb(row, on_item_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        items[i] = row;

        badges[i] = lv_label_create(row);
        lv_obj_set_style_text_font(badges[i], &lv_font_montserrat_18, 0);
        lv_obj_set_pos(badges[i], 12, 8);

        names[i] = lv_label_create(row);
        lv_obj_set_style_text_font(names[i], &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(names[i], COLOR_WHITE, 0);
        lv_obj_set_width(names[i], SCREEN_WIDTH - 20 - 58 - 12);
        lv_label_set_long_mode(names[i], LV_LABEL_LONG_DOT);
        lv_obj_set_pos(names[i], 58, 8);

        descs[i] = lv_label_create(row);
        lv_obj_set_style_text_font(descs[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(descs[i], COLOR_WHITE, 0);
        lv_obj_set_width(descs[i], SCREEN_WIDTH - 20 - 58 - 12);
        lv_label_set_long_mode(descs[i], LV_LABEL_LONG_DOT);
        lv_obj_set_pos(descs[i], 58, 33);
    }
}

static void apply_mode() {
    lv_label_set_text(lbl_title, manage_mode ? "MODIFICA PROGRAMMI" : "PROGRAMMI");
    lv_label_set_text(lbl_hint, manage_mode ? "Tocca un programma per modificarlo"
                                            : "Tocca un programma per caricarlo");
}

// ============================================
// Hide programs screen
// ============================================
void ui_programs_hide() {
    programs_visible = false;
    if (manage_mode) {
        ui_settings_show();
    } else {
        // Return to the correct screen based on active mode
        if (ui_auto_is_visible() && ui_auto_get_screen()) {
            lv_scr_load(ui_auto_get_screen());
        } else {
            lv_scr_load(ui_main_get_screen());
        }
    }
    Serial.println("Programs screen hidden");
}

// ============================================
// Check visibility
// ============================================
bool ui_programs_is_visible() {
    return programs_visible;
}

// ============================================
// Refresh list after edit
// ============================================
void ui_programs_refresh() {
    if (programs_visible && screen_programs) {
        update_list();
    }
}

// ============================================
// Destroy screen to free memory
// ============================================
void ui_programs_destroy() {
    if (screen_programs) {
        lv_obj_del(screen_programs);
        screen_programs = nullptr;
        list_container = nullptr;
        lbl_title = nullptr;
        lbl_hint = nullptr;
        for (int i = 0; i < MAX_PROGRAMS; i++) {
            items[i] = nullptr;
            badges[i] = nullptr;
            names[i] = nullptr;
            descs[i] = nullptr;
        }
    }
}

// ============================================
// Update list items
// ============================================

// "▲ 380°   ▼ 300°   5:00   2 fasi"
static void describe(uint8_t idx, char* buf, size_t len) {
    const Program* p = &g_programs[idx];
    if (!programs_is_configured(idx)) {
        snprintf(buf, len, "Vuoto");
        return;
    }
    const Phase& ph = p->phases[0];

    char tm[16];
    if (idx == PROGRAM_MANUAL && idx == programs_get_current()) {
        // P0 caricato: il timer vero (anche 1:30)
        snprintf(tm, sizeof(tm), "%d:%02d", g_state.timer_minutes, g_state.timer_seconds);
    } else if (ph.duration > 0) {
        snprintf(tm, sizeof(tm), "%d:00", ph.duration);
    } else {
        snprintf(tm, sizeof(tm), "senza timer");
    }

    // Cielo con la freccia su, platea con la freccia giu' (come BOOST in PRO)
    if (p->num_phases > 1) {
        snprintf(buf, len, LV_SYMBOL_UP " %d\xC2\xB0   " LV_SYMBOL_DOWN " %d\xC2\xB0   %s   %d fasi",
                 ph.temp_cielo, ph.temp_platea, tm, p->num_phases);
    } else {
        snprintf(buf, len, LV_SYMBOL_UP " %d\xC2\xB0   " LV_SYMBOL_DOWN " %d\xC2\xB0   %s",
                 ph.temp_cielo, ph.temp_platea, tm);
    }
}

static void update_list() {
    uint8_t current = programs_get_current();
    char buf[64];

    for (int i = 0; i < MAX_PROGRAMS; i++) {
        snprintf(buf, sizeof(buf), "P%d", i);
        lv_label_set_text(badges[i], buf);

        bool is_current = (i == current);
        lv_obj_set_style_text_color(badges[i], is_current ? COLOR_ORANGE : COLOR_WHITE, 0);
        lv_obj_set_style_border_color(items[i], is_current ? COLOR_ORANGE : COLOR_DARK_GRAY, 0);
        lv_obj_set_style_border_width(items[i], is_current ? 2 : 1, 0);

        const char* name = g_programs[i].name;
        lv_label_set_text(names[i], name[0] ? name : "(senza nome)");

        describe(i, buf, sizeof(buf));
        lv_label_set_text(descs[i], buf);
    }

    // Apre l'elenco sul programma attivo (P12 non sta nella prima schermata)
    lv_obj_update_layout(list_container);
    lv_obj_scroll_to_view(items[manage_mode ? 0 : current], LV_ANIM_OFF);
}

// ============================================
// Event callbacks
// ============================================
static void on_back_click(lv_event_t* e) {
    buzzer_beep();
    ui_programs_hide();
}

static void on_item_click(lv_event_t* e) {
    buzzer_beep();
    uint8_t prog_idx = (uint8_t)(intptr_t)lv_event_get_user_data(e);
    if (prog_idx >= MAX_PROGRAMS) return;

    if (manage_mode) {
        // IMPORTANTE: prima caricare la nuova schermata, poi eliminare la vecchia
        // Altrimenti LVGL crasha perche' eliminiamo la schermata attiva
        ui_program_edit_show(prog_idx);
        ui_programs_destroy();  // Ora screen_programs non e' piu' attiva, possiamo eliminarla
    } else {
        programs_set_current(prog_idx);
        buzzer_confirm();
        ui_programs_hide();
    }
}
