/**
 * Settings Screen Implementation
 * Forno Pizza Project
 *
 * Un solo elenco (niente pagine 1/3): ogni riga mostra il valore attuale,
 * le voci usate spesso stanno in alto. Suono, WiFi e Sistema aprono una
 * sottopagina; la freccia in alto torna all'elenco e da li' alla home.
 *
 * Stile unico: righe scure con bordo sottile, testo bianco, valori in
 * arancio, rosso solo per il reset di fabbrica.
 */

#include "ui_settings.h"
#include "ui_mode.h"
#include "ui_keypad.h"
#include "ui_theme.h"
#include "ui_main.h"
#include "ui_widgets.h"
#include "ui_programs.h"
#include "ui_wifi.h"
#include "ui_ota.h"
#include "thermal_profile.h"
#include "cooldown.h"
#include "buzzer.h"
#include "fan.h"
#include "hardware_pins.h"
#include "settings_storage.h"
#include "oven_state.h"
#include "wifi_time.h"
#include "wifi_storage.h"
#include "schedule.h"

// ============================================
// Pagine
// ============================================
enum SettingsPage { PAGE_MAIN = 0, PAGE_SOUND, PAGE_WIFI, PAGE_SYSTEM, PAGE_SCHEDULE, PAGE_COUNT };

static const char* PAGE_TITLES[PAGE_COUNT] = {"IMPOSTAZIONI", "SUONO", "WIFI", "SISTEMA",
                                             "AVVIO PROGRAMMATO"};

#define SETTINGS_HEADER_H  50
#define ROW_X              10
#define ROW_W              (SCREEN_WIDTH - 2 * ROW_X)
#define ROW_H              52
#define ROW_STEP           60

// ============================================
// Static UI elements
// ============================================
static lv_obj_t* screen_settings = nullptr;
static lv_obj_t* lbl_title = nullptr;
static lv_obj_t* pages[PAGE_COUNT] = {nullptr};
static int s_page = PAGE_MAIN;
static bool settings_visible = false;

// Elenco principale
static lv_obj_t* val_soak = nullptr;
static lv_obj_t* val_sound = nullptr;
static lv_obj_t* val_wifi = nullptr;
static lv_obj_t* btn_mode_simple = nullptr;
static lv_obj_t* btn_mode_pro = nullptr;
static lv_obj_t* val_sched = nullptr;
static int s_soak_edit = 0;          // valore in editing dal keypad (0 = AUTO)

// Avvio programmato: valori in editing dal keypad, applicati da update
static lv_obj_t* sw_sched = nullptr;
static lv_obj_t* val_sch_time = nullptr;
static lv_obj_t* val_sch_temp = nullptr;
static lv_obj_t* val_sch_dur = nullptr;
static lv_obj_t* lbl_sch_info = nullptr;
static int s_sch_hour_edit = 0;
static int s_sch_min_edit = 0;
static int s_sch_temp_edit = 0;
static int s_sch_dur_min_edit = 0;
static int s_sch_dur_sec_edit = 0;

// Suono
static lv_obj_t* sw_sound = nullptr;
static lv_obj_t* slider_volume = nullptr;
static lv_obj_t* lbl_volume_val = nullptr;

// WiFi
static lv_obj_t* lbl_wifi_info = nullptr;
static lv_obj_t* sw_wifi = nullptr;

// Sistema
static lv_obj_t* lbl_sys_info = nullptr;
static lv_obj_t* sw_tach = nullptr;
static lv_obj_t* lbl_reset = nullptr;
static unsigned long s_reset_done_ms = 0;

// ============================================
// Forward declarations
// ============================================
static void on_back_click(lv_event_t* e);
static void on_page_click(lv_event_t* e);
static void on_soak_click(lv_event_t* e);
static void on_sched_toggle(lv_event_t* e);
static void on_sch_time_click(lv_event_t* e);
static void on_sch_temp_click(lv_event_t* e);
static void on_sch_dur_click(lv_event_t* e);
static void load_sched_edits();
static void on_programs_click(lv_event_t* e);
static void on_mode_click(lv_event_t* e);
static void on_sound_toggle(lv_event_t* e);
static void on_volume_change(lv_event_t* e);
static void on_test_alarm_click(lv_event_t* e);
static void on_lamour_click(lv_event_t* e);
static void on_wifi_toggle(lv_event_t* e);
static void on_wifi_config_click(lv_event_t* e);
static void on_tach_toggle(lv_event_t* e);
static void on_ota_click(lv_event_t* e);
static bool reset_armed();
static void reset_hold();
static void reset_short();
static void refresh_values();
static void sync_controls();

// Reset di fabbrica: sempre da tenere premuto 3 s (niente finestra di conferma)
static HoldButton s_reset_hold = {reset_armed, nullptr, reset_hold, reset_short,
                                  nullptr, 0, false, false, 3000};

// ============================================
// Helpers di costruzione
// ============================================
static void set_text_if_changed(lv_obj_t* lbl, const char* txt) {
    if (lbl && strcmp(lv_label_get_text(lbl), txt) != 0) lv_label_set_text(lbl, txt);
}

static lv_obj_t* create_page() {
    lv_obj_t* p = lv_obj_create(screen_settings);
    lv_obj_remove_style_all(p);
    lv_obj_set_size(p, SCREEN_WIDTH, SCREEN_HEIGHT - SETTINGS_HEADER_H);
    lv_obj_set_pos(p, 0, SETTINGS_HEADER_H);
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
    return p;
}

// Riga: [icona] Titolo ............ valore/controllo
static lv_obj_t* create_row(lv_obj_t* parent, int y, const char* icon, const char* title,
                            lv_event_cb_t on_click = nullptr, void* user_data = nullptr) {
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, ROW_W, ROW_H);
    lv_obj_set_pos(row, ROW_X, y);
    lv_obj_set_style_bg_color(row, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(row, COLOR_DARK_GRAY, 0);
    lv_obj_set_style_border_width(row, 1, 0);
    lv_obj_set_style_radius(row, 10, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    if (on_click) {
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(row, COLOR_DARK_GRAY, LV_STATE_PRESSED);
        lv_obj_add_event_cb(row, on_click, LV_EVENT_CLICKED, user_data);
    } else {
        lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);
    }

    int text_x = 14;
    if (icon) {
        lv_obj_t* ic = lv_label_create(row);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_18, 0);
        lv_obj_set_style_text_color(ic, COLOR_WHITE, 0);
        lv_label_set_text(ic, icon);
        lv_obj_align(ic, LV_ALIGN_LEFT_MID, 14, 0);
        text_x = 46;
    }

    lv_obj_t* lbl = lv_label_create(row);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
    lv_label_set_text(lbl, title);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, text_x, 0);
    return row;
}

static void add_chevron(lv_obj_t* row) {
    lv_obj_t* ch = lv_label_create(row);
    lv_obj_set_style_text_font(ch, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(ch, COLOR_WHITE, 0);
    lv_label_set_text(ch, LV_SYMBOL_RIGHT);
    lv_obj_align(ch, LV_ALIGN_RIGHT_MID, -12, 0);
}

// Valore in arancio, a sinistra della freccia (o del bordo se senza freccia)
static lv_obj_t* add_value(lv_obj_t* row, bool with_chevron) {
    lv_obj_t* v = lv_label_create(row);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(v, COLOR_ORANGE, 0);
    lv_label_set_text(v, "");
    lv_obj_align(v, LV_ALIGN_RIGHT_MID, with_chevron ? -34 : -14, 0);
    return v;
}

static lv_obj_t* add_switch(lv_obj_t* row, lv_event_cb_t cb) {
    lv_obj_t* sw = lv_switch_create(row);
    lv_obj_set_size(sw, 56, 30);
    lv_obj_align(sw, LV_ALIGN_RIGHT_MID, -12, 0);
    lv_obj_set_style_bg_color(sw, COLOR_DARK_GRAY, 0);
    lv_obj_set_style_bg_color(sw, COLOR_GREEN_READY, LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw, cb, LV_EVENT_VALUE_CHANGED, NULL);
    return sw;
}

// Riquadro informativo: chiavi a sinistra in grigio, valori a destra in bianco
static lv_obj_t* create_info_box(lv_obj_t* parent, int y, int h, const char* keys) {
    lv_obj_t* box = lv_obj_create(parent);
    lv_obj_remove_style_all(box);
    lv_obj_set_size(box, ROW_W, h);
    lv_obj_set_pos(box, ROW_X, y);
    lv_obj_set_style_bg_color(box, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_bg_opa(box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(box, COLOR_DARK_GRAY, 0);
    lv_obj_set_style_border_width(box, 1, 0);
    lv_obj_set_style_radius(box, 10, 0);
    lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* k = lv_label_create(box);
    lv_obj_set_style_text_font(k, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(k, COLOR_WHITE, 0);
    lv_obj_set_style_text_line_space(k, 8, 0);
    lv_label_set_text(k, keys);
    lv_obj_set_pos(k, 14, 12);

    lv_obj_t* v = lv_label_create(box);
    lv_obj_set_style_text_font(v, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(v, COLOR_WHITE, 0);
    lv_obj_set_style_text_line_space(v, 8, 0);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_width(v, ROW_W - 130);
    lv_label_set_text(v, "");
    lv_obj_set_pos(v, 116, 12);
    return v;
}

// ============================================
// Costruzione delle pagine
// ============================================
static void build_main_page() {
    lv_obj_t* p = pages[PAGE_MAIN];
    int y = 12;

    lv_obj_t* r = create_row(p, y, LV_SYMBOL_CHARGE, "Attesa pietra", on_soak_click);
    val_soak = add_value(r, true);
    add_chevron(r);
    y += ROW_STEP;

    r = create_row(p, y, LV_SYMBOL_BELL, "Avvio programmato", on_page_click,
                   (void*)(intptr_t)PAGE_SCHEDULE);
    val_sched = add_value(r, true);
    add_chevron(r);
    y += ROW_STEP;

    r = create_row(p, y, LV_SYMBOL_LIST, "Programmi", on_programs_click);
    add_chevron(r);
    y += ROW_STEP;

    // Schermata: selettore SEMPLICE | PRO che mostra quella attiva
    r = create_row(p, y, LV_SYMBOL_SHUFFLE, "Schermata");
    const char* names[2] = {"SEMPLICE", "PRO"};
    int widths[2] = {92, 56};
    int x = -8;
    for (int i = 1; i >= 0; i--) {
        lv_obj_t* b = lv_btn_create(r);
        lv_obj_set_size(b, widths[i], 36);
        lv_obj_align(b, LV_ALIGN_RIGHT_MID, x, 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_border_width(b, 1, 0);
        lv_obj_set_style_border_color(b, COLOR_DARK_GRAY, 0);
        lv_obj_add_event_cb(b, on_mode_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_t* l = lv_label_create(b);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
        lv_label_set_text(l, names[i]);
        lv_obj_center(l);
        if (i == 0) btn_mode_simple = b; else btn_mode_pro = b;
        x -= widths[i] + 6;
    }
    y += ROW_STEP;

    r = create_row(p, y, LV_SYMBOL_VOLUME_MAX, "Suono", on_page_click, (void*)(intptr_t)PAGE_SOUND);
    val_sound = add_value(r, true);
    add_chevron(r);
    y += ROW_STEP;

    r = create_row(p, y, LV_SYMBOL_WIFI, "WiFi", on_page_click, (void*)(intptr_t)PAGE_WIFI);
    val_wifi = add_value(r, true);
    add_chevron(r);
    y += ROW_STEP;

    r = create_row(p, y, LV_SYMBOL_SETTINGS, "Sistema", on_page_click, (void*)(intptr_t)PAGE_SYSTEM);
    lv_obj_t* v = add_value(r, true);
    lv_label_set_text(v, "v" FW_VERSION);
    add_chevron(r);
}

static void build_sound_page() {
    lv_obj_t* p = pages[PAGE_SOUND];
    int y = 12;

    lv_obj_t* r = create_row(p, y, LV_SYMBOL_VOLUME_MAX, "Suono");
    sw_sound = add_switch(r, on_sound_toggle);
    y += ROW_STEP;

    // Volume: riga alta con il cursore sotto
    lv_obj_t* vol = create_row(p, y, nullptr, "Volume");
    lv_obj_set_height(vol, 92);
    lv_obj_t* title = lv_obj_get_child(vol, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);
    lbl_volume_val = lv_label_create(vol);
    lv_obj_set_style_text_font(lbl_volume_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_volume_val, COLOR_ORANGE, 0);
    lv_obj_align(lbl_volume_val, LV_ALIGN_TOP_RIGHT, -14, 12);

    slider_volume = lv_slider_create(vol);
    lv_obj_set_size(slider_volume, ROW_W - 48, 14);
    lv_obj_align(slider_volume, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_slider_set_range(slider_volume, BUZZER_VOLUME_MIN, BUZZER_VOLUME_MAX);
    lv_obj_set_style_bg_color(slider_volume, COLOR_DARK_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(slider_volume, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(slider_volume, COLOR_WHITE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(slider_volume, 6, LV_PART_KNOB);
    // Area di tocco piu' ampia del cursore sottile
    lv_obj_set_ext_click_area(slider_volume, 16);
    lv_obj_add_event_cb(slider_volume, on_volume_change, LV_EVENT_VALUE_CHANGED, NULL);
    y += 92 + 8;

    create_row(p, y, LV_SYMBOL_BELL, "Prova allarme", on_test_alarm_click);
    y += ROW_STEP;

    create_row(p, y, LV_SYMBOL_AUDIO, "L'amour Toujours", on_lamour_click);
}

static void build_wifi_page() {
    lv_obj_t* p = pages[PAGE_WIFI];
    int y = 12;

    lbl_wifi_info = create_info_box(p, y, 150, "Stato\nRete\nIndirizzo\nIP\nSegnale");
    y += 150 + 10;

    lv_obj_t* r = create_row(p, y, nullptr, "WiFi sempre attivo");
    sw_wifi = add_switch(r, on_wifi_toggle);
    y += ROW_STEP;

    r = create_row(p, y, LV_SYMBOL_EDIT, "Configura rete", on_wifi_config_click);
    add_chevron(r);
}

static void build_system_page() {
    lv_obj_t* p = pages[PAGE_SYSTEM];
    int y = 12;

    lbl_sys_info = create_info_box(p, y, 124, "Firmware\nIP\nTemp. box\nVentola");
    y += 124 + 10;

    lv_obj_t* r = create_row(p, y, nullptr, "Sicurezza ventola");
    sw_tach = add_switch(r, on_tach_toggle);
    y += ROW_STEP;

    r = create_row(p, y, LV_SYMBOL_DOWNLOAD, "Aggiorna firmware", on_ota_click);
    add_chevron(r);

    // Reset di fabbrica: in fondo, rosso, tenere premuto 3 s
    lv_obj_t* btn = lv_btn_create(p);
    lv_obj_set_size(btn, ROW_W, ROW_H);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_set_style_bg_color(btn, COLOR_RED_DARK, 0);
    lv_obj_set_style_radius(btn, 10, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lbl_reset = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl_reset, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_reset, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(lbl_reset, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_reset, "Reset di fabbrica\n(tieni premuto 3 s)");
    lv_obj_center(lbl_reset);
    ui_widget_attach_hold(btn, &s_reset_hold);
}

static void build_schedule_page() {
    lv_obj_t* p = pages[PAGE_SCHEDULE];
    int y = 12;

    lv_obj_t* r = create_row(p, y, LV_SYMBOL_POWER, "Attivo");
    sw_sched = add_switch(r, on_sched_toggle);
    y += ROW_STEP;

    r = create_row(p, y, nullptr, "Ora di accensione", on_sch_time_click);
    val_sch_time = add_value(r, true);
    add_chevron(r);
    y += ROW_STEP;

    r = create_row(p, y, nullptr, "Temperatura", on_sch_temp_click);
    val_sch_temp = add_value(r, true);
    add_chevron(r);
    y += ROW_STEP;

    r = create_row(p, y, nullptr, "Timer", on_sch_dur_click);
    val_sch_dur = add_value(r, true);
    add_chevron(r);
    y += ROW_STEP + 4;

    // Cosa succedera' (o cosa e' successo all'ultimo avvio)
    lbl_sch_info = lv_label_create(p);
    lv_obj_set_style_text_font(lbl_sch_info, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sch_info, COLOR_LIGHT_GRAY, 0);
    lv_obj_set_style_text_line_space(lbl_sch_info, 6, 0);
    lv_label_set_long_mode(lbl_sch_info, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_sch_info, ROW_W - 8);
    lv_obj_set_pos(lbl_sch_info, ROW_X + 4, y);
    lv_label_set_text(lbl_sch_info, "");
}

static void create_screen() {
    screen_settings = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_settings, COLOR_BG, 0);
    lv_obj_clear_flag(screen_settings, LV_OBJ_FLAG_SCROLLABLE);

    // Header: indietro + titolo della pagina
    lv_obj_t* header = lv_obj_create(screen_settings);
    lv_obj_remove_style_all(header);
    lv_obj_set_size(header, SCREEN_WIDTH, SETTINGS_HEADER_H);
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
    lv_label_set_text(lbl_title, PAGE_TITLES[PAGE_MAIN]);
    lv_obj_align(lbl_title, LV_ALIGN_CENTER, 12, 0);

    for (int i = 0; i < PAGE_COUNT; i++) pages[i] = create_page();
    build_main_page();
    build_sound_page();
    build_wifi_page();
    build_system_page();
    build_schedule_page();
}

static void show_page(int page) {
    if (page < 0 || page >= PAGE_COUNT) page = PAGE_MAIN;
    s_page = page;
    for (int i = 0; i < PAGE_COUNT; i++) {
        if (i == page) lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(lbl_title, PAGE_TITLES[page]);
    refresh_values();
}

// ============================================
// Show / hide
// ============================================
void ui_settings_open() {
    s_page = PAGE_MAIN;
    ui_settings_show();
}

void ui_settings_show() {
    settings_visible = true;
    s_soak_edit = settings_get_soak_override();  // evita salvataggi spuri
    load_sched_edits();

    if (!screen_settings) create_screen();

    sync_controls();
    show_page(s_page);
    lv_scr_load(screen_settings);
}

void ui_settings_hide() {
    settings_visible = false;
    extern bool ui_auto_is_visible();
    extern lv_obj_t* ui_auto_get_screen();
    if (ui_auto_is_visible()) {
        lv_scr_load(ui_auto_get_screen());
    } else {
        lv_scr_load(ui_main_get_screen());
    }
}

// Ritorno dal tastierino aperto dalle impostazioni: ricarica lo schermo cosi'
// com'e' (pagina corrente, valore appena confermato salvato da update)
void ui_settings_return() {
    if (screen_settings) lv_scr_load(screen_settings);
}

bool ui_settings_is_visible() {
    return settings_visible;
}

// ============================================
// Valori
// ============================================

// Interruttori e cursore dallo stato salvato (all'apertura e dopo un reset)
static void sync_controls() {
    if (buzzer_is_enabled()) lv_obj_add_state(sw_sound, LV_STATE_CHECKED);
    else lv_obj_clear_state(sw_sound, LV_STATE_CHECKED);
    lv_slider_set_value(slider_volume, buzzer_get_volume(), LV_ANIM_OFF);

    if (settings_get_tach_enabled()) lv_obj_add_state(sw_tach, LV_STATE_CHECKED);
    else lv_obj_clear_state(sw_tach, LV_STATE_CHECKED);

    if (settings_get_wifi_always_on()) lv_obj_add_state(sw_wifi, LV_STATE_CHECKED);
    else lv_obj_clear_state(sw_wifi, LV_STATE_CHECKED);

    if (schedule_get_state() != ScheduleState::OFF) lv_obj_add_state(sw_sched, LV_STATE_CHECKED);
    else lv_obj_clear_state(sw_sched, LV_STATE_CHECKED);
}

static void load_sched_edits() {
    s_sch_hour_edit = schedule_get_hour();
    s_sch_min_edit = schedule_get_minute();
    s_sch_temp_edit = schedule_get_temp();
    s_sch_dur_min_edit = schedule_get_duration_s() / 60;
    s_sch_dur_sec_edit = schedule_get_duration_s() % 60;
}

static const char* rssi_quality(int rssi) {
    if (rssi >= -60) return "ottimo";
    if (rssi >= -70) return "buono";
    if (rssi >= -80) return "debole";
    return "scarso";
}

static void style_mode_btn(lv_obj_t* b, bool active) {
    lv_obj_set_style_bg_color(b, active ? COLOR_ORANGE : COLOR_BUTTON_BG, 0);
    lv_obj_set_style_text_color(lv_obj_get_child(b, 0), COLOR_WHITE, 0);
}

// Testi che cambiano da soli (RPM, WiFi, temperatura...): solo se diversi
static void refresh_values() {
    char buf[160];
    bool connected = wifi_time_is_connected();

    // --- Elenco ---
    int soak = settings_get_soak_override();
    if (soak <= 0) snprintf(buf, sizeof(buf), "AUTO");
    else snprintf(buf, sizeof(buf), "%d min", soak);
    set_text_if_changed(val_soak, buf);

    if (buzzer_is_enabled()) snprintf(buf, sizeof(buf), "%d%%", buzzer_get_volume());
    else snprintf(buf, sizeof(buf), "Muto");
    set_text_if_changed(val_sound, buf);

    ScheduleState sch = schedule_get_state();
    if (sch == ScheduleState::ARMED) {
        snprintf(buf, sizeof(buf), "%02d:%02d", schedule_get_hour(), schedule_get_minute());
    } else if (sch != ScheduleState::OFF) {
        snprintf(buf, sizeof(buf), "In corso");
    } else {
        snprintf(buf, sizeof(buf), "OFF");
    }
    set_text_if_changed(val_sched, buf);

    if (connected) snprintf(buf, sizeof(buf), "pyro.local");
    else if (!settings_get_wifi_always_on()) snprintf(buf, sizeof(buf), "Spento");
    else snprintf(buf, sizeof(buf), "Non connesso");
    set_text_if_changed(val_wifi, buf);

    static int s_prev_pro = -1;
    int pro = ui_mode_is_pro() ? 1 : 0;
    if (pro != s_prev_pro) {
        s_prev_pro = pro;
        style_mode_btn(btn_mode_simple, !pro);
        style_mode_btn(btn_mode_pro, pro);
    }

    // --- Suono ---
    snprintf(buf, sizeof(buf), "%d%%", buzzer_get_volume());
    set_text_if_changed(lbl_volume_val, buf);

    // --- WiFi (solo quando la pagina e' aperta) ---
    if (s_page == PAGE_WIFI) {
        char ssid[33];
        wifi_storage_get_ssid(ssid, sizeof(ssid));
        ssid[16] = '\0';  // una riga sola: un nome lungo andrebbe a capo
        const char* state;
        if (connected) state = "Connesso";
        else if (wifi_time_get_status() == WiFiStatus::CONNECTING) state = "Connessione...";
        else if (!settings_get_wifi_always_on()) state = "Spento";
        else state = "Non connesso";

        char signal[24];
        if (connected) {
            int rssi = wifi_time_get_rssi();
            snprintf(signal, sizeof(signal), "%s (%d dBm)", rssi_quality(rssi), rssi);
        } else {
            snprintf(signal, sizeof(signal), "--");
        }
        snprintf(buf, sizeof(buf), "%s\n%s\n%s\n%s\n%s",
                 state, ssid[0] ? ssid : "--",
                 connected ? "pyro.local" : "--",
                 connected ? wifi_time_get_ip() : "--",
                 signal);
        set_text_if_changed(lbl_wifi_info, buf);
    }

    // --- Avvio programmato (solo quando la pagina e' aperta) ---
    if (s_page == PAGE_SCHEDULE) {
        // Lo stato cambia anche da solo (avvio scattato) o dal web
        bool on = (sch != ScheduleState::OFF);
        if (on != lv_obj_has_state(sw_sched, LV_STATE_CHECKED)) {
            if (on) lv_obj_add_state(sw_sched, LV_STATE_CHECKED);
            else lv_obj_clear_state(sw_sched, LV_STATE_CHECKED);
        }

        snprintf(buf, sizeof(buf), "%02d:%02d", schedule_get_hour(), schedule_get_minute());
        set_text_if_changed(val_sch_time, buf);
        snprintf(buf, sizeof(buf), "%d\xC2\xB0" "C", schedule_get_temp());
        set_text_if_changed(val_sch_temp, buf);
        int d = schedule_get_duration_s();
        snprintf(buf, sizeof(buf), "%d:%02d", d / 60, d % 60);
        set_text_if_changed(val_sch_dur, buf);

        if (sch == ScheduleState::ARMED) {
            // Dopo un riavvio, finche' l'ora NTP non torna, il "fra" non si sa
            int left = schedule_minutes_to_start();
            char fra[20] = "";
            if (left >= 0) snprintf(fra, sizeof(fra), " (fra %dh%02dm)", left / 60, left % 60);
            snprintf(buf, sizeof(buf),
                     "Si accende alle %02d:%02d%s. "
                     "Il timer parte appena arriva in temperatura, "
                     "poi il forno si spegne.",
                     schedule_get_hour(), schedule_get_minute(), fra);
        } else if (sch == ScheduleState::HEATING) {
            snprintf(buf, sizeof(buf),
                     "Acceso, in riscaldamento. "
                     "Il timer parte appena arriva in temperatura.");
        } else if (sch == ScheduleState::TIMING) {
            snprintf(buf, sizeof(buf),
                     "Timer in corso: a zero il forno si spegne. "
                     "Disattivando, resta acceso.");
        } else if (!wifi_time_is_synced()) {
            snprintf(buf, sizeof(buf), "Serve l'ora da internet: collega il WiFi.");
        } else if (schedule_get_note()[0]) {
            snprintf(buf, sizeof(buf), "Ultimo avvio: %s.", schedule_get_note());
        } else {
            snprintf(buf, sizeof(buf),
                     "Si accende all'ora scelta, avvia il timer appena "
                     "arriva in temperatura e a fine timer si spegne. "
                     "Vale una volta.");
        }
        set_text_if_changed(lbl_sch_info, buf);
    }

    // --- Sistema ---
    if (s_page == PAGE_SYSTEM) {
        snprintf(buf, sizeof(buf), "v" FW_VERSION "\n%s\n%d\xC2\xB0" "C\n%d RPM",
                 connected ? wifi_time_get_ip() : "--",
                 g_state.temp_ambient, fan_tach_get_rpm());
        set_text_if_changed(lbl_sys_info, buf);

        if (s_reset_done_ms && millis() - s_reset_done_ms > 3000) {
            s_reset_done_ms = 0;
            lv_label_set_text(lbl_reset, "Reset di fabbrica\n(tieni premuto 3 s)");
        }
    }
}

// ============================================
// Event callbacks
// ============================================
static void on_back_click(lv_event_t* e) {
    buzzer_beep();
    if (s_page != PAGE_MAIN) {
        show_page(PAGE_MAIN);
    } else {
        ui_settings_hide();
    }
}

static void on_page_click(lv_event_t* e) {
    buzzer_beep();
    show_page((int)(intptr_t)lv_event_get_user_data(e));
}

static void on_soak_click(lv_event_t* e) {
    buzzer_beep();
    s_soak_edit = settings_get_soak_override();
    // 0 = automatico (valore appreso dal forno)
    ui_keypad_show(&s_soak_edit, "ATTESA PIETRA (0=AUTO)", 0, SOAK_OVERRIDE_MAX, "min");
}

static void on_sched_toggle(lv_event_t* e) {
    bool checked = lv_obj_has_state(sw_sched, LV_STATE_CHECKED);
    if (checked) {
        if (schedule_arm()) {
            buzzer_confirm();
        } else {
            // Senza ora NTP non si sa quando accendere: l'interruttore torna giu'
            buzzer_error();
            lv_obj_clear_state(sw_sched, LV_STATE_CHECKED);
        }
    } else {
        buzzer_beep();
        schedule_cancel();
    }
    refresh_values();
}

static void on_sch_time_click(lv_event_t* e) {
    buzzer_beep();
    load_sched_edits();
    ui_keypad_show_clock(&s_sch_hour_edit, &s_sch_min_edit, "ORA DI ACCENSIONE");
}

static void on_sch_temp_click(lv_event_t* e) {
    buzzer_beep();
    load_sched_edits();
    ui_keypad_show(&s_sch_temp_edit, "TEMPERATURA", 50, 500);
}

static void on_sch_dur_click(lv_event_t* e) {
    buzzer_beep();
    load_sched_edits();
    ui_keypad_show_time(&s_sch_dur_min_edit, &s_sch_dur_sec_edit, "TIMER (m:ss)");
}

static void on_programs_click(lv_event_t* e) {
    buzzer_beep();
    ui_programs_show_manage();  // Apre in modalita' gestione
}

static void on_mode_click(lv_event_t* e) {
    bool to_pro = (int)(intptr_t)lv_event_get_user_data(e) == 1;
    if (to_pro == ui_mode_is_pro()) return;  // gia' attiva
    buzzer_beep();
    // Il cambio non altera lo stato del forno (vive in g_state, non nello
    // schermo): consentito anche a forno acceso. Si esce dalle impostazioni
    settings_visible = false;
    s_page = PAGE_MAIN;
    ui_mode_request_switch(to_pro);  // eseguito nel main loop
}

static void on_sound_toggle(lv_event_t* e) {
    bool checked = lv_obj_has_state(sw_sound, LV_STATE_CHECKED);
    buzzer_set_enabled(checked);
    if (checked) buzzer_beep();  // Feedback when enabling
    Serial.printf("Sound: %s\n", checked ? "ON" : "OFF");
}

static void on_volume_change(lv_event_t* e) {
    buzzer_set_volume(lv_slider_get_value(slider_volume));
    refresh_values();

    // Test beep when adjusting volume
    static unsigned long last_beep = 0;
    if (millis() - last_beep > 300) {
        buzzer_beep();
        last_beep = millis();
    }
}

static void on_test_alarm_click(lv_event_t* e) {
    buzzer_alert();
    Serial.println("Settings: test alarm");
}

static void on_lamour_click(lv_event_t* e) {
    buzzer_lamour_toujours();
    Serial.println("Settings: L'amour Toujours!");
}

static void on_wifi_toggle(lv_event_t* e) {
    bool checked = lv_obj_has_state(sw_wifi, LV_STATE_CHECKED);
    settings_set_wifi_always_on(checked);
    buzzer_beep();

    if (checked) {
        if (!wifi_time_is_connected()) wifi_time_reconnect();
    } else {
        wifi_time_disconnect();
    }
    Serial.printf("WiFi always-on: %s\n", checked ? "ON" : "OFF");
}

static void on_wifi_config_click(lv_event_t* e) {
    buzzer_beep();
    ui_wifi_show();  // al ritorno si riapre la pagina WiFi
}

static void on_tach_toggle(lv_event_t* e) {
    bool checked = lv_obj_has_state(sw_tach, LV_STATE_CHECKED);
    settings_set_tach_enabled(checked);
    buzzer_beep();
    Serial.printf("TACH control: %s\n", checked ? "ON" : "OFF");
}

static void on_ota_click(lv_event_t* e) {
    buzzer_beep();
    ui_ota_show();
}

static bool reset_armed() {
    return true;
}

static void reset_short() {
    buzzer_beep();
    lv_label_set_text(lbl_reset, "Tieni premuto 3 secondi");
    s_reset_done_ms = millis();  // il testo torna normale dopo 3 s
}

static void reset_hold() {
    buzzer_alert();
    settings_factory_reset();
    thermal_profile_reset();
    cooldown_reset();

    // Ricarica impostazioni nel buzzer
    buzzer_set_enabled(settings_get_sound_enabled());
    buzzer_set_volume(settings_get_volume());
    s_soak_edit = settings_get_soak_override();

    sync_controls();
    refresh_values();
    lv_label_set_text(lbl_reset, LV_SYMBOL_OK " Impostazioni ripristinate");
    s_reset_done_ms = millis();
    Serial.println("UI: Reset di fabbrica completato");
}

// ============================================
// Update dynamic elements (called from main loop)
// ============================================
void ui_settings_update() {
    if (!settings_visible || !screen_settings) return;

    // Attesa pietra confermata dal keypad
    if (s_soak_edit != settings_get_soak_override()) {
        settings_set_soak_override(s_soak_edit);
        Serial.printf("Settings: attesa pietra = %d min (0=auto)\n", s_soak_edit);
    }

    // Avvio programmato confermato dal keypad
    if (s_sch_hour_edit != schedule_get_hour() || s_sch_min_edit != schedule_get_minute()) {
        schedule_set_time(s_sch_hour_edit, s_sch_min_edit);
    }
    if (s_sch_temp_edit != schedule_get_temp()) {
        schedule_set_temp(s_sch_temp_edit);
    }
    int dur = s_sch_dur_min_edit * 60 + s_sch_dur_sec_edit;
    if (dur != schedule_get_duration_s()) {
        schedule_set_duration_s(dur);  // 0:00 rifiutato dentro
    }
    load_sched_edits();  // allinea anche ai cambi fatti dal web

    refresh_values();
}
