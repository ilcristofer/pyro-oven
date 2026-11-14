/**
 * SEMPLICE Mode Main Screen - Ultra-simple interface
 * Single temperature, ACCENDI/SPEGNI, optional timer.
 * Never enters COOKING - stays in MAINTAIN/READY for autotune.
 * v3.26: Complete rewrite - truly simple mode
 */

#include "ui_auto.h"
#include "ui_theme.h"
#include "ui_keypad.h"
#include "ui_settings.h"
#include "ui_widgets.h"
#include "ui_mode.h"
#include "oven_state.h"
#include "hardware_pins.h"
#include "wifi_time.h"
#include "buzzer.h"
#include "settings_storage.h"
#include "thermal_profile.h"
#include "cooldown.h"
#include "schedule.h"

// Font custom 72pt per temperatura grande
LV_FONT_DECLARE(lv_font_montserrat_72);

// ============================================
// Taratura SEMPLICE: dalla temperatura scelta T agli obiettivi delle sonde
// cielo = T + offset_cielo(T), platea = T - offset_platea(T)
//
// Il cibo appoggia sulla pietra: sotto cuoce per contatto, sopra solo per
// aria e irraggiamento, che a pari temperatura scaldano 3-4 volte meno.
// Misure IR del 3/11 (SEMPLICE 200, vecchia taratura +5/-32): superficie
// della pietra 202, cioe' quanto la temperatura mostrata, e sonda platea
// (sotto la pietra) 18-30 gradi piu' fredda della superficie. Risultato:
// fondo dorato e sopra chiaro (lasagna finita col boost, cordon bleu da
// girare). Fino a 250 il cielo sta 20 sopra e la pietra ~20-30 sotto T.
// Crostata del 4/11 a 180 (platea 134, pietra misurata 160): sopra bene,
// fondo cotto ma non abbastanza anche con 5 min di boost platea finale.
// Platea alzata di ~12 gradi: a 180 sonda 145, pietra ~170 (resta sotto T).
//
// Sopra i 300 e' la taratura pizza, invariata: la resistenza incandescente
// irraggia gia' abbastanza e il cielo ha poco margine (asintoto ~440).
// Tra 250 e 300 si passa da una all'altra.
//
// La temperatura mostrata a forno acceso e' cielo - offset_cielo(T) a regime,
// con lo scostamento ridotto in proporzione durante la salita, anche su web
// e app (campi smt/smo/smd di /api/status)
// ============================================
struct SimpleCalPoint { int16_t t; int8_t cielo; int8_t platea; };
static const SimpleCalPoint SIMPLE_CAL[] = {
    // T     cielo  platea
    { 100,   20,    28 },
    { 150,   20,    30 },
    { 200,   20,    38 },
    { 250,   20,    45 },
    { 300,    5,    48 },
    { 400,    5,    55 },
};
static const int SIMPLE_CAL_N = sizeof(SIMPLE_CAL) / sizeof(SIMPLE_CAL[0]);

// Interpolazione lineare tra i punti, costante fuori dalla tabella
static int simple_cal(int target, bool cielo) {
    const SimpleCalPoint* p = SIMPLE_CAL;
    if (target <= p[0].t) return cielo ? p[0].cielo : p[0].platea;
    for (int i = 1; i < SIMPLE_CAL_N; i++) {
        if (target <= p[i].t) {
            int a = cielo ? p[i - 1].cielo : p[i - 1].platea;
            int b = cielo ? p[i].cielo : p[i].platea;
            int span = p[i].t - p[i - 1].t;
            int num = (b - a) * (target - p[i - 1].t);
            // arrotondamento al grado piu' vicino, anche per valori negativi
            int frac = (num >= 0 ? num + span / 2 : num - span / 2) / span;
            return a + frac;
        }
    }
    const SimpleCalPoint& last = p[SIMPLE_CAL_N - 1];
    return cielo ? last.cielo : last.platea;
}

int ui_auto_offset_cielo(int target) { return simple_cal(target, true); }
static int simple_offset_platea(int target) { return simple_cal(target, false); }

// Lo scostamento mostrato cresce con la temperatura: zero a forno freddo,
// pieno quando la sonda arriva al suo obiettivo. Con lo scostamento pieno
// fisso, a forno appena acceso si leggeva cielo 21 - 20 = 1 grado.
// Resta monotona finche' lo scostamento e' minore della salita (sempre qui).
#define SIMPLE_SHOW_AMBIENT 20
static int scaled_offset(int offset, int temp, int probe_target) {
    int span = probe_target - SIMPLE_SHOW_AMBIENT;
    if (span <= 0 || offset == 0) return offset;
    int rise = temp - SIMPLE_SHOW_AMBIENT;
    if (rise <= 0) return 0;
    if (rise >= span) return offset;
    return offset * rise / span;
}

static void apply_simple_targets(int target) {
    g_state.target_cielo = target + ui_auto_offset_cielo(target);
    g_state.target_platea = target - simple_offset_platea(target);
}

// ============================================
// Static state
// ============================================
static bool s_auto_visible = false;
static int s_target_temp = 200;
static int s_last_timer_s = 0;          // ultimo timer avviato (secondi)
static bool s_ready_buzzed = false;

int ui_auto_shown_offset_cielo() {
    int off = ui_auto_offset_cielo(s_target_temp);
    return scaled_offset(off, g_state.temp_cielo, s_target_temp + off);
}

int ui_auto_display_temp() {
    if (g_state.error_cielo && g_state.error_platea) return -1000;
    if (g_state.error_cielo) {
        int off = simple_offset_platea(s_target_temp);
        return g_state.temp_platea +
            scaled_offset(off, g_state.temp_platea, s_target_temp - off);
    }
    return g_state.temp_cielo - ui_auto_shown_offset_cielo();
}

// Tocco breve su SPEGNI: per un attimo il pulsante dice "TIENI PREMUTO"
#define HOLD_HINT_MS 1500
static bool s_hold_hint = false;
static unsigned long s_hold_hint_start = 0;

// Label change guards (avoid redundant lv_label_set_text at 10Hz)
static int s_prev_minute = -1;
static int s_prev_temp_ambient = -999;
static bool s_prev_sound_enabled = true;
static int s_prev_target = -1;
static int s_prev_timer_min = -1;
static int s_prev_timer_sec = -1;
static int s_prev_display_temp = -999;
static int s_prev_status_layout = -1;   // 0 testo, 1 testo + sonde, 2 solo sonde

// ============================================
// Static UI elements
// ============================================
static lv_obj_t* screen_auto = nullptr;

// Header
static lv_obj_t* lbl_wifi = nullptr;
static lv_obj_t* lbl_datetime = nullptr;
static lv_obj_t* lbl_temp_box = nullptr;
static lv_obj_t* lbl_sound = nullptr;

// Status
static lv_obj_t* lbl_status = nullptr;

// Detail panel (cielo/platea temp+power, shown when PRONTO)
static lv_obj_t* cont_detail = nullptr;
static lv_obj_t* lbl_detail_cielo = nullptr;
static lv_obj_t* lbl_detail_platea = nullptr;

// Current temperature
static lv_obj_t* lbl_temp_big = nullptr;

// Target temperature
static lv_obj_t* btn_temp_minus = nullptr;
static lv_obj_t* lbl_target = nullptr;
static lv_obj_t* btn_target_tap = nullptr;
static lv_obj_t* btn_temp_plus = nullptr;

// Timer
static lv_obj_t* lbl_timer_icon = nullptr;
static lv_obj_t* lbl_timer = nullptr;
static lv_obj_t* btn_timer_tap = nullptr;
static lv_obj_t* btn_timer_play = nullptr;

// Action button
static lv_obj_t* btn_action = nullptr;
static lv_obj_t* lbl_action = nullptr;

// Boost buttons
static lv_obj_t* btn_boost_cielo = nullptr;
static lv_obj_t* lbl_boost_cielo = nullptr;
static lv_obj_t* btn_boost_platea = nullptr;
static lv_obj_t* lbl_boost_platea = nullptr;

// Menu button
static lv_obj_t* btn_menu = nullptr;

// Label change guards for boost / layout
static bool s_prev_oven_on_layout = false;
static bool s_prev_boost_active_cielo = false;
static bool s_prev_boost_active_platea = false;

// ============================================
// Forward declarations
// ============================================
static void create_header(lv_obj_t* parent);
static void create_status_zone(lv_obj_t* parent);
static void create_temp_display(lv_obj_t* parent);
static void create_target_zone(lv_obj_t* parent);
static void create_timer_zone(lv_obj_t* parent);
static void create_action_button(lv_obj_t* parent);
static void create_boost_buttons(lv_obj_t* parent);
static void create_menu_button(lv_obj_t* parent);

static void on_temp_plus_click(lv_event_t* e);
static void on_temp_minus_click(lv_event_t* e);
static void on_target_tap_click(lv_event_t* e);
static void on_timer_tap_click(lv_event_t* e);
static void on_timer_play_click(lv_event_t* e);
static void on_timer_reset_click(lv_event_t* e);
// ACCENDI e' un tocco, SPEGNI una pressione di 1s (ui_widget_attach_hold)
static bool action_is_armed();
static void action_tap();
static void action_hold();
static void action_short();
static HoldButton s_action_hold = {action_is_armed, action_tap, action_hold, action_short,
                                   nullptr, 0, false, false};
static void on_boost_cielo_click(lv_event_t* e);
static void on_boost_platea_click(lv_event_t* e);
static void on_menu_click(lv_event_t* e);

static void apply_target(int new_target);
static int hottest_temp();

// ============================================
// Create SEMPLICE mode screen
// ============================================
void ui_auto_create() {
    s_auto_visible = true;
    s_ready_buzzed = false;

    // Reset label guards to force full refresh on new/reloaded screen
    s_prev_minute = -1;
    s_prev_temp_ambient = -999;
    s_prev_sound_enabled = !buzzer_is_enabled();
    s_prev_target = -1;
    s_prev_timer_min = -1;
    s_prev_timer_sec = -1;
    s_prev_display_temp = -999;
    s_prev_status_layout = -1;
    s_prev_oven_on_layout = false;
    s_prev_boost_active_cielo = false;
    s_prev_boost_active_platea = false;

    // Load persisted values
    s_target_temp = settings_load_simple_target();
    s_last_timer_s = settings_load_simple_timer_s();
    s_hold_hint = false;

    // Apply target to g_state with offset
    apply_simple_targets(s_target_temp);

    // Load timer (non sopra un timer in corso: il cambio di modalita' non deve
    // alterare una cottura)
    if (s_last_timer_s > 0 && !g_state.timer_active) {
        g_state.timer_minutes = s_last_timer_s / 60;
        g_state.timer_seconds = s_last_timer_s % 60;
    }

    if (screen_auto) {
        lv_scr_load(screen_auto);
        return;
    }

    screen_auto = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_auto, COLOR_BG, 0);
    lv_obj_clear_flag(screen_auto, LV_OBJ_FLAG_SCROLLABLE);

    create_header(screen_auto);
    create_status_zone(screen_auto);
    create_temp_display(screen_auto);
    create_target_zone(screen_auto);
    create_timer_zone(screen_auto);
    create_action_button(screen_auto);
    create_boost_buttons(screen_auto);
    create_menu_button(screen_auto);

    lv_scr_load(screen_auto);
    Serial.println("[SEMPLICE] Screen created (v3.26 ultra-simple)");
}

// ============================================
// Header (Y=0, H=35)
// ============================================
static void create_header(lv_obj_t* parent) {
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, HEADER_Y);
    lv_obj_set_style_bg_color(header, COLOR_BG, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    lbl_wifi = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_wifi, COLOR_GRAY, 0);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_pos(lbl_wifi, 8, (HEADER_HEIGHT - 14) / 2);

    lbl_datetime = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_datetime, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_datetime, COLOR_WHITE, 0);
    lv_label_set_text(lbl_datetime, "");
    lv_obj_set_pos(lbl_datetime, 10, (HEADER_HEIGHT - 14) / 2);

    lbl_temp_box = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_temp_box, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_temp_box, COLOR_WHITE, 0);
    lv_label_set_text(lbl_temp_box, "");
    // Allineato a destra (prima dell'altoparlante): segue la larghezza del testo
    lv_obj_align(lbl_temp_box, LV_ALIGN_RIGHT_MID, -32, 0);

    lbl_sound = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_sound, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_sound, COLOR_WHITE, 0);
    lv_label_set_text(lbl_sound, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_pos(lbl_sound, SCREEN_WIDTH - 25, (HEADER_HEIGHT - 14) / 2);
}

// ============================================
// Status zone (Y=35, H=60)
// Text: "SPENTO" / "RISCALDAMENTO ~5min" / "TEMPO SCADUTO!"
// Detail: cielo/platea temp+power (when PRONTO)
// ============================================

// Helper: create small cielo/platea icon (white rect with black line top/bottom)
static lv_obj_t* create_detail_icon(lv_obj_t* parent, bool is_cielo) {
    lv_obj_t* rect = lv_obj_create(parent);
    lv_obj_set_size(rect, 22, 14);
    lv_obj_set_style_bg_color(rect, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0);
    lv_obj_set_style_border_width(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_clear_flag(rect, LV_OBJ_FLAG_SCROLLABLE);

    // Black line on top (cielo) or bottom (platea)
    lv_obj_t* line = lv_obj_create(rect);
    lv_obj_set_size(line, 16, 3);
    lv_obj_set_style_bg_color(line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_pos(line, 3, is_cielo ? 1 : 10);

    return rect;
}

static void create_status_zone(lv_obj_t* parent) {
    // Text label (SPENTO, RISCALDAMENTO, TEMPO SCADUTO)
    lbl_status = lv_label_create(parent);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_status, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(lbl_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_status, SCREEN_WIDTH - 20);
    lv_label_set_text(lbl_status, "SPENTO");
    lv_obj_set_pos(lbl_status, 10, 50);

    // Detail container (cielo/platea temp+power on one row, shown when PRONTO)
    cont_detail = lv_obj_create(parent);
    lv_obj_set_size(cont_detail, SCREEN_WIDTH - 20, 30);
    lv_obj_set_pos(cont_detail, 10, 52);
    lv_obj_set_style_bg_opa(cont_detail, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_detail, 0, 0);
    lv_obj_set_style_pad_all(cont_detail, 0, 0);
    lv_obj_clear_flag(cont_detail, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(cont_detail, LV_OBJ_FLAG_HIDDEN);

    // Single row: [icon_cielo] 250° 60%    [icon_platea] 230° 55%
    lv_obj_t* icon_c = create_detail_icon(cont_detail, true);
    lv_obj_set_pos(icon_c, 5, 8);

    lbl_detail_cielo = lv_label_create(cont_detail);
    lv_obj_set_style_text_font(lbl_detail_cielo, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_detail_cielo, COLOR_WHITE, 0);
    lv_label_set_text(lbl_detail_cielo, "--- 0%");
    lv_obj_set_pos(lbl_detail_cielo, 30, 5);

    lv_obj_t* icon_p = create_detail_icon(cont_detail, false);
    lv_obj_set_pos(icon_p, 165, 8);

    lbl_detail_platea = lv_label_create(cont_detail);
    lv_obj_set_style_text_font(lbl_detail_platea, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_detail_platea, COLOR_WHITE, 0);
    lv_label_set_text(lbl_detail_platea, "--- 0%");
    lv_obj_set_pos(lbl_detail_platea, 190, 5);
}

// ============================================
// Current temperature display (Y=95, H=120)
// Big font showing average temperature
// ============================================
static void create_temp_display(lv_obj_t* parent) {
    // Numero e "C" nella stessa riga centrata: l'unita' resta attaccata al
    // numero qualunque sia il numero di cifre (prima stava fissa a destra)
    lv_obj_t* row = lv_obj_create(parent);
    lv_obj_remove_style_all(row);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(row, 4, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(row, LV_ALIGN_TOP_MID, 0, 115);

    lbl_temp_big = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_temp_big, &lv_font_montserrat_72, 0);
    lv_obj_set_style_text_color(lbl_temp_big, COLOR_WHITE, 0);
    lv_label_set_text(lbl_temp_big, "---");

    lv_obj_t* lbl_unit = lv_label_create(row);
    lv_obj_set_style_text_font(lbl_unit, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_unit, COLOR_WHITE, 0);
    lv_obj_set_style_pad_top(lbl_unit, 12, 0);
    lv_label_set_text(lbl_unit, "\xC2\xB0" "C");
}

// ============================================
// Target temperature zone (Y=215, H=55)
// [-5]  200°C  [+5]
// ============================================
static void create_target_zone(lv_obj_t* parent) {
    int y = 215;
    int btn_w = 70;
    int btn_h = 50;

    // -5 button
    btn_temp_minus = lv_btn_create(parent);
    lv_obj_set_size(btn_temp_minus, btn_w, btn_h);
    lv_obj_set_pos(btn_temp_minus, 15, y);
    lv_obj_set_style_bg_color(btn_temp_minus, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(btn_temp_minus, 10, 0);
    lv_obj_set_style_border_width(btn_temp_minus, 1, 0);
    lv_obj_set_style_border_color(btn_temp_minus, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_temp_minus, on_temp_minus_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_m = lv_label_create(btn_temp_minus);
    lv_obj_set_style_text_font(lbl_m, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_m, COLOR_WHITE, 0);
    lv_label_set_text(lbl_m, "- 5");
    lv_obj_center(lbl_m);

    // Target label (center, tappable for keypad)
    btn_target_tap = lv_obj_create(parent);
    lv_obj_set_size(btn_target_tap, 120, btn_h);
    lv_obj_set_pos(btn_target_tap, (SCREEN_WIDTH - 120) / 2, y);
    lv_obj_set_style_bg_opa(btn_target_tap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_target_tap, 0, 0);
    lv_obj_set_style_pad_all(btn_target_tap, 0, 0);
    lv_obj_clear_flag(btn_target_tap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn_target_tap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_target_tap, on_target_tap_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_set_txt = lv_label_create(btn_target_tap);
    lv_obj_set_style_text_font(lbl_set_txt, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_set_txt, COLOR_WHITE, 0);
    lv_label_set_text(lbl_set_txt, "SET");
    lv_obj_align(lbl_set_txt, LV_ALIGN_TOP_MID, 0, 2);

    lbl_target = lv_label_create(btn_target_tap);
    lv_obj_set_style_text_font(lbl_target, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_target, COLOR_ORANGE, 0);
    lv_label_set_text(lbl_target, "200\xC2\xB0");
    lv_obj_align(lbl_target, LV_ALIGN_BOTTOM_MID, 0, -2);

    // +5 button
    btn_temp_plus = lv_btn_create(parent);
    lv_obj_set_size(btn_temp_plus, btn_w, btn_h);
    lv_obj_set_pos(btn_temp_plus, SCREEN_WIDTH - btn_w - 15, y);
    lv_obj_set_style_bg_color(btn_temp_plus, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(btn_temp_plus, 10, 0);
    lv_obj_set_style_border_width(btn_temp_plus, 1, 0);
    lv_obj_set_style_border_color(btn_temp_plus, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_temp_plus, on_temp_plus_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_p = lv_label_create(btn_temp_plus);
    lv_obj_set_style_text_font(lbl_p, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_p, COLOR_WHITE, 0);
    lv_label_set_text(lbl_p, "+ 5");
    lv_obj_center(lbl_p);
}

// ============================================
// Timer zone (Y=275, H=55)
// clock icon  15:00  [play/pause]
// ============================================
static void create_timer_zone(lv_obj_t* parent) {
    int y = 278;

    // Reset timer: stesso riquadro del play (prima era solo un'icona e non
    // sembrava un pulsante)
    lv_obj_t* btn_timer_reset = lv_btn_create(parent);
    lv_obj_set_size(btn_timer_reset, 50, 50);
    lv_obj_set_pos(btn_timer_reset, 40, y);
    lv_obj_set_style_bg_color(btn_timer_reset, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(btn_timer_reset, 10, 0);
    lv_obj_set_style_border_width(btn_timer_reset, 1, 0);
    lv_obj_set_style_border_color(btn_timer_reset, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_timer_reset, on_timer_reset_click, LV_EVENT_CLICKED, NULL);

    lbl_timer_icon = lv_label_create(btn_timer_reset);
    lv_obj_set_style_text_font(lbl_timer_icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_timer_icon, COLOR_ORANGE, 0);
    lv_label_set_text(lbl_timer_icon, LV_SYMBOL_REFRESH);
    lv_obj_center(lbl_timer_icon);

    // Timer label (tappable for keypad)
    btn_timer_tap = lv_obj_create(parent);
    lv_obj_set_size(btn_timer_tap, 120, 50);
    lv_obj_set_pos(btn_timer_tap, (SCREEN_WIDTH - 120) / 2, y);
    lv_obj_set_style_bg_opa(btn_timer_tap, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_timer_tap, 0, 0);
    lv_obj_set_style_pad_all(btn_timer_tap, 0, 0);
    lv_obj_clear_flag(btn_timer_tap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn_timer_tap, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_timer_tap, on_timer_tap_click, LV_EVENT_CLICKED, NULL);

    lbl_timer = lv_label_create(btn_timer_tap);
    lv_obj_set_style_text_font(lbl_timer, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_timer, COLOR_WHITE, 0);
    lv_label_set_text(lbl_timer, "0:00");
    lv_obj_center(lbl_timer);

    // Play/pause button
    btn_timer_play = lv_btn_create(parent);
    lv_obj_set_size(btn_timer_play, 50, 50);
    lv_obj_set_pos(btn_timer_play, SCREEN_WIDTH - 90, y);
    lv_obj_set_style_bg_color(btn_timer_play, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(btn_timer_play, 10, 0);
    lv_obj_set_style_border_width(btn_timer_play, 1, 0);
    lv_obj_set_style_border_color(btn_timer_play, lv_color_hex(0x555555), 0);
    lv_obj_add_event_cb(btn_timer_play, on_timer_play_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_play = lv_label_create(btn_timer_play);
    lv_obj_set_style_text_font(lbl_play, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_play, COLOR_GREEN, 0);
    lv_label_set_text(lbl_play, LV_SYMBOL_PLAY);
    lv_obj_center(lbl_play);
}

// ============================================
// Action button (Y=340, H=65)
// ACCENDI (green) / SPEGNI (red)
// ============================================
static void create_action_button(lv_obj_t* parent) {
    int btn_w = SCREEN_WIDTH - 40;
    int btn_h = 65;
    int y = 340;

    btn_action = lv_btn_create(parent);
    lv_obj_set_size(btn_action, btn_w, btn_h);
    lv_obj_set_pos(btn_action, 20, y);
    lv_obj_set_style_bg_color(btn_action, COLOR_GREEN_DARK, 0);
    lv_obj_set_style_radius(btn_action, 12, 0);
    ui_widget_attach_hold(btn_action, &s_action_hold);

    lbl_action = lv_label_create(btn_action);
    lv_obj_set_style_text_font(lbl_action, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_action, COLOR_WHITE, 0);
    lv_label_set_text(lbl_action, "ACCENDI");
    lv_obj_center(lbl_action);
}

// ============================================
// Boost buttons (Y=390, H=44) - hidden when oven off
// ============================================
static void create_boost_buttons(lv_obj_t* parent) {
    int y = 386;
    int h = 44;
    int gap = 8;
    // Il servizio pizze non e' in SEMPLICE (resta in PRO e nelle impostazioni)
    int btn_w = (SCREEN_WIDTH - 40 - gap) / 2;  // 136px ciascuno

    // Boost Cielo
    btn_boost_cielo = lv_btn_create(parent);
    lv_obj_set_size(btn_boost_cielo, btn_w, h);
    lv_obj_set_pos(btn_boost_cielo, 20, y);
    lv_obj_set_style_bg_color(btn_boost_cielo, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(btn_boost_cielo, 10, 0);
    lv_obj_set_style_border_width(btn_boost_cielo, 1, 0);
    lv_obj_set_style_border_color(btn_boost_cielo, lv_color_hex(0x555555), 0);
    lv_obj_add_flag(btn_boost_cielo, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_boost_cielo, on_boost_cielo_click, LV_EVENT_CLICKED, NULL);

    lbl_boost_cielo = lv_label_create(btn_boost_cielo);
    lv_obj_set_style_text_font(lbl_boost_cielo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_boost_cielo, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(lbl_boost_cielo, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_boost_cielo, "BOOST\nCIELO");
    lv_obj_center(lbl_boost_cielo);

    // Boost Platea
    btn_boost_platea = lv_btn_create(parent);
    lv_obj_set_size(btn_boost_platea, btn_w, h);
    lv_obj_set_pos(btn_boost_platea, 20 + btn_w + gap, y);
    lv_obj_set_style_bg_color(btn_boost_platea, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(btn_boost_platea, 10, 0);
    lv_obj_set_style_border_width(btn_boost_platea, 1, 0);
    lv_obj_set_style_border_color(btn_boost_platea, lv_color_hex(0x555555), 0);
    lv_obj_add_flag(btn_boost_platea, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(btn_boost_platea, on_boost_platea_click, LV_EVENT_CLICKED, NULL);

    lbl_boost_platea = lv_label_create(btn_boost_platea);
    lv_obj_set_style_text_font(lbl_boost_platea, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_boost_platea, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(lbl_boost_platea, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(lbl_boost_platea, "BOOST\nPLATEA");
    lv_obj_center(lbl_boost_platea);
}

// ============================================
// Menu button (Y=420, H=50)
// ============================================
static void create_menu_button(lv_obj_t* parent) {
    int y = 420;

    btn_menu = lv_btn_create(parent);
    lv_obj_set_size(btn_menu, SCREEN_WIDTH - 40, 50);
    lv_obj_set_pos(btn_menu, 20, y);
    lv_obj_set_style_bg_color(btn_menu, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(btn_menu, 10, 0);
    lv_obj_set_style_border_width(btn_menu, 1, 0);
    lv_obj_set_style_border_color(btn_menu, COLOR_GRAY, 0);
    lv_obj_add_event_cb(btn_menu, on_menu_click, LV_EVENT_CLICKED, NULL);
    ui_mode_attach_menu_long_press(btn_menu);  // hold 1s -> passa a PRO

    lv_obj_t* lbl = lv_label_create(btn_menu);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS " MENU");
    lv_obj_center(lbl);
}

// ============================================
// Destroy
// ============================================
void ui_auto_destroy() {
    s_auto_visible = false;
    if (screen_auto) {
        lv_obj_del(screen_auto);
        screen_auto = nullptr;
    }
}

lv_obj_t* ui_auto_get_screen() {
    return screen_auto;
}

bool ui_auto_is_visible() {
    return s_auto_visible;
}

// ============================================
// Apply target temperature
// ============================================
static void apply_target(int new_target) {
    new_target = constrain(new_target, 50, 500);
    s_target_temp = new_target;
    apply_simple_targets(new_target);
    settings_save_simple_target(new_target);
}

// La temperatura piu' alta tra le sonde funzionanti (-1000 = entrambe in errore):
// a forno spento conta quanto e' ancora caldo, non il target
static int hottest_temp() {
    if (g_state.error_cielo && g_state.error_platea) return -1000;
    if (g_state.error_cielo) return g_state.temp_platea;
    if (g_state.error_platea) return g_state.temp_cielo;
    return max(g_state.temp_cielo, g_state.temp_platea);
}

// ============================================
// Azioni condivise touch/web (nessuna chiamata LVGL)
// ============================================
void ui_auto_set_target(int target) {
    apply_target(target);
}

int ui_auto_get_target() {
    return s_target_temp;
}

void ui_auto_power_on() {
    if (g_state.error_fan) {
        buzzer_stop();
        g_state.error_fan = false;
        g_state.error_msg_fan[0] = '\0';
        g_state.error_count_fan = 0;
    }
    s_ready_buzzed = false;
    // Il target SEMPLICE salvato, anche se lo schermo SEMPLICE non e' attivo
    if (!s_auto_visible) s_target_temp = settings_load_simple_target();
    apply_simple_targets(s_target_temp);
    g_state.power_on = true;
    oven_state_start_preheat();
    Serial.printf("[SEMPLICE] ACCENDI -> PREHEAT %d\xC2\xB0" "C\n", s_target_temp);
}

void ui_auto_timer_play() {
    if (g_state.timer_active) {
        g_state.timer_paused = !g_state.timer_paused;
    } else if (g_state.timer_minutes > 0 || g_state.timer_seconds > 0) {
        oven_state_timer_start();  // alla scadenza torna a questo valore
        s_last_timer_s = g_state.timer_minutes * 60 + g_state.timer_seconds;
        settings_save_simple_timer_s(s_last_timer_s);
    }
}

void ui_auto_timer_reset() {
    // Ferma il timer e torna all'ultimo tempo usato, pronto per la pizza
    // dopo. Se era gia' fermo su quel valore, azzera
    bool running = g_state.timer_active || g_state.timer_paused || g_state.timer_expired;
    int cur = g_state.timer_minutes * 60 + g_state.timer_seconds;
    int t = (running || cur != s_last_timer_s) ? s_last_timer_s : 0;

    g_state.timer_active = false;
    g_state.timer_paused = false;
    g_state.timer_expired = false;
    g_state.timer_blink = false;
    g_state.timer_minutes = t / 60;
    g_state.timer_seconds = t % 60;
}

void ui_auto_toggle_boost(bool cielo) {
    if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) return;
    // Toggle senza countdown: lo spegne l'utente. Il limite di sicurezza
    // (BOOST_MAX_OVER sopra il target) sta in oven_state_resistance_*_on()
    if (cielo) {
        g_state.boost_cielo = !g_state.boost_cielo;
        g_state.boost_time_cielo = 0;  // no countdown, update_boost() non lo tocca
    } else {
        g_state.boost_platea = !g_state.boost_platea;
        g_state.boost_time_platea = 0;
    }
    oven_state_update_effective_power();
    Serial.printf("[SEMPLICE] Boost %s: %s\n", cielo ? "Cielo" : "Platea",
                  (cielo ? g_state.boost_cielo : g_state.boost_platea) ? "ON" : "OFF");
}

// ============================================
// Update (called 10Hz from main loop)
// ============================================
void ui_auto_update() {
    if (!screen_auto || !s_auto_visible) return;

    char buf[64];

    // --- Sync target from keypad changes (keypad writes to s_target_temp) ---
    int expected_cielo = s_target_temp + ui_auto_offset_cielo(s_target_temp);
    int expected_platea = s_target_temp - simple_offset_platea(s_target_temp);
    if (g_state.target_cielo != expected_cielo || g_state.target_platea != expected_platea) {
        apply_simple_targets(s_target_temp);
        settings_save_simple_target(s_target_temp);
    }

    // --- Header ---
    WiFiStatus wifi_status = wifi_time_get_status();
    bool time_synced = wifi_time_is_synced();
    bool wifi_icon_visible = false;

    if (wifi_status == WiFiStatus::CONNECTING) {
        lv_obj_set_style_text_color(lbl_wifi,
            g_state.resistance_blink_on ? COLOR_ORANGE : COLOR_GRAY, 0);
        lv_obj_clear_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
        wifi_icon_visible = true;
    } else if (wifi_status == WiFiStatus::CONNECTED) {
        lv_obj_set_style_text_color(lbl_wifi, COLOR_GREEN, 0);
        lv_obj_clear_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
        wifi_icon_visible = true;
    } else {
        lv_obj_add_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
    }

    if (time_synced) {
        if (g_state.minute != s_prev_minute) {
            s_prev_minute = g_state.minute;
            sprintf(buf, "%s %02d.%02d.%04d - %02d:%02d",
                    oven_state_get_day_name(g_state.day_of_week),
                    g_state.day, g_state.month, g_state.year,
                    g_state.hour, g_state.minute);
            lv_label_set_text(lbl_datetime, buf);
        }
        lv_obj_set_pos(lbl_datetime, wifi_icon_visible ? 30 : 10, (HEADER_HEIGHT - 14) / 2);
    } else {
        lv_label_set_text(lbl_datetime, "");
        s_prev_minute = -1;
    }

    if (g_state.temp_ambient != s_prev_temp_ambient) {
        s_prev_temp_ambient = g_state.temp_ambient;
        // "BOX": e' la temperatura dell'elettronica, non del forno
        snprintf(buf, sizeof(buf), "BOX %d\xC2\xB0", g_state.temp_ambient);
        lv_label_set_text(lbl_temp_box, buf);
    }

    bool sound_on = buzzer_is_enabled();
    if (sound_on != s_prev_sound_enabled) {
        s_prev_sound_enabled = sound_on;
        lv_label_set_text(lbl_sound, sound_on ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);
    }

    // --- Status zone ---
    // A forno acceso cielo e platea si vedono sempre: in salita, stabilizzazione,
    // ripresa e boost il titolo va su una riga e le sonde sotto; a PRONTO solo le sonde
    bool show_detail = false;
    bool compact = false;
    int s_deficit = 0, s_deficit_eta = 0;
    if (!oven_state_temp_deficit(&s_deficit, &s_deficit_eta)) {
        s_deficit = 0;
    }

    if (g_state.error_fan) {
        // Ventola ferma o scheda surriscaldata: il motivo e' nel messaggio
        snprintf(buf, sizeof(buf), "ERRORE\n%s", g_state.error_msg_fan);
        lv_label_set_text(lbl_status, buf);
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
    } else if (g_state.power_on && (g_state.error_cielo || g_state.error_platea)) {
        // Con una sonda guasta la sua resistenza e' spenta e il forno non
        // arriva mai a PRONTO: va detto, non lasciato in "RISCALDAMENTO"
        snprintf(buf, sizeof(buf), "ERRORE SONDA\n%s",
                 g_state.error_cielo ? "CIELO" : "PLATEA");
        lv_label_set_text(lbl_status, buf);
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
    } else if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) {
        // Spento ma ancora caldo: la pietra resta a centinaia di gradi per ore,
        // si dice quanto manca a poterlo toccare o coprire
        int cool = cooldown_get_minutes();
        if (schedule_get_state() == ScheduleState::ARMED) {
            // Si accendera' da solo: va detto, prima di tutto il resto
            sprintf(buf, "SPENTO\nAVVIO ALLE %02d:%02d",
                    schedule_get_hour(), schedule_get_minute());
            lv_label_set_text(lbl_status, buf);
            lv_obj_set_style_text_color(lbl_status, COLOR_ORANGE, 0);
        } else if (cool > 0) {
            if (cool >= 60) {
                sprintf(buf, "SPENTO\nFREDDO FRA ~%dh%02dm", cool / 60, cool % 60);
            } else {
                sprintf(buf, "SPENTO\nFREDDO FRA ~%dmin", cool);
            }
            lv_label_set_text(lbl_status, buf);
            lv_obj_set_style_text_color(lbl_status,
                hottest_temp() >= HOT_WARN_TEMP ? COLOR_ORANGE : COLOR_WHITE, 0);
        } else if (hottest_temp() >= HOT_WARN_TEMP) {
            lv_label_set_text(lbl_status, "SPENTO\nFORNO CALDO");
            lv_obj_set_style_text_color(lbl_status, COLOR_ORANGE, 0);
        } else {
            lv_label_set_text(lbl_status, "SPENTO");
            lv_obj_set_style_text_color(lbl_status, COLOR_WHITE, 0);
        }
    } else if (g_state.timer_expired && g_state.timer_blink) {
        lv_label_set_text(lbl_status, "TEMPO SCADUTO!");
        lv_obj_set_style_text_color(lbl_status, COLOR_ORANGE, 0);
        compact = true;
    } else if (g_state.mode == OvenMode::BOOST) {
        lv_label_set_text(lbl_status, "BOOST");
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
        compact = true;
    } else if (g_state.recovery_active || (g_state.stone_ready && s_deficit > 0)) {
        // Dopo un'infornata la camera resta sotto il setpoint per minuti:
        // meglio dire quanto manca che dichiarare il forno pronto
        if (s_deficit > 0) {
            sprintf(buf, "IN RIPRESA  -%d\xC2\xB0 ~%dmin", s_deficit, s_deficit_eta);
        } else {
            sprintf(buf, "RECUPERO");
        }
        lv_label_set_text(lbl_status, buf);
        lv_obj_set_style_text_color(lbl_status, COLOR_RED, 0);
        compact = true;
    } else if (g_state.mode == OvenMode::READY) {
        // PRONTO (pietra carica): show cielo/platea detail
        show_detail = true;
    } else {
        // Salita delle sonde o saturazione della pietra: in entrambi i casi
        // si mostra il tempo al forno davvero pronto per infornare
        int eta = thermal_profile_get_ready_eta_minutes();
        bool soaking = g_state.cielo_in_maintain && g_state.platea_in_maintain;
        const char* titolo = soaking ? "STABILIZZAZIONE" : "RISCALDAMENTO";

        if (eta > 0) {
            if (eta >= 60) {
                sprintf(buf, "%s ~%dh%02dm", titolo, eta / 60, eta % 60);
            } else {
                sprintf(buf, "%s ~%dmin", titolo, eta);
            }
        } else {
            sprintf(buf, "%s...", titolo);
        }
        lv_label_set_text(lbl_status, buf);
        lv_obj_set_style_text_color(lbl_status, COLOR_ORANGE, 0);
        compact = true;
    }

    // Disposizione: stile e posizioni cambiano solo quando cambia lo stato
    int layout = show_detail ? 2 : (compact ? 1 : 0);
    if (layout != s_prev_status_layout) {
        s_prev_status_layout = layout;
        if (layout == 2) {
            // PRONTO: solo cielo/platea
            lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cont_detail, 10, 52);
            lv_obj_clear_flag(cont_detail, LV_OBJ_FLAG_HIDDEN);
        } else if (layout == 1) {
            // Titolo su una riga (font 20: "STABILIZZAZIONE ~1h05m" sta in
            // 300 px) e sonde sotto, sopra la temperatura grande (y 115)
            lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_20, 0);
            lv_obj_set_pos(lbl_status, 10, 42);
            lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(cont_detail, 10, 70);
            lv_obj_clear_flag(cont_detail, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Spento o errore: testo grande su due righe
            lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_24, 0);
            lv_obj_set_pos(lbl_status, 10, 50);
            lv_obj_clear_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(cont_detail, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (layout != 0) {
        // Update cielo/platea info
        int pw_c = oven_state_get_effective_power_cielo();
        int pw_p = oven_state_get_effective_power_platea();
        sprintf(buf, "%d\xC2\xB0  %d%%", g_state.temp_cielo, pw_c);
        lv_label_set_text(lbl_detail_cielo, buf);
        sprintf(buf, "%d\xC2\xB0  %d%%", g_state.temp_platea, pw_p);
        lv_label_set_text(lbl_detail_platea, buf);
    }

    // Ready buzzer (once per preheat cycle)
    if (g_state.mode == OvenMode::READY && !s_ready_buzzed) {
        s_ready_buzzed = true;
        buzzer_alert();
    }

    // --- Big temperature ---
    // Acceso: cielo - offset (confrontabile con il target utente).
    // Spento: la temperatura vera piu' alta, in grigio (prima "---" anche a 400C)
    {
        int display_temp;
        bool dim = false;
        if (g_state.error_cielo && g_state.error_platea) {
            display_temp = -1000;  // ERR sentinel
        } else if (g_state.power_on && g_state.mode != OvenMode::STANDBY) {
            display_temp = ui_auto_display_temp();
        } else {
            display_temp = hottest_temp();
            dim = true;
        }

        // Il guard tiene conto anche del colore (acceso/spento)
        int key = dim ? display_temp + 100000 : display_temp;
        if (key != s_prev_display_temp) {
            s_prev_display_temp = key;
            if (display_temp == -1000) {
                // Il font da 72 contiene solo cifre: "ERR" non si vedeva
                lv_obj_set_style_text_font(lbl_temp_big, &lv_font_montserrat_48, 0);
                lv_label_set_text(lbl_temp_big, "ERR");
                lv_obj_set_style_text_color(lbl_temp_big, COLOR_RED, 0);
            } else {
                sprintf(buf, "%d", display_temp);
                lv_obj_set_style_text_font(lbl_temp_big, &lv_font_montserrat_72, 0);
                lv_label_set_text(lbl_temp_big, buf);
                lv_obj_set_style_text_color(lbl_temp_big, dim ? COLOR_GRAY : COLOR_WHITE, 0);
            }
        }
    }

    // --- Target temperature ---
    if (s_target_temp != s_prev_target) {
        s_prev_target = s_target_temp;
        sprintf(buf, "%d\xC2\xB0", s_target_temp);
        lv_label_set_text(lbl_target, buf);
    }

    // --- Timer ---
    if (g_state.timer_minutes != s_prev_timer_min || g_state.timer_seconds != s_prev_timer_sec) {
        s_prev_timer_min = g_state.timer_minutes;
        s_prev_timer_sec = g_state.timer_seconds;
        sprintf(buf, "%d:%02d", g_state.timer_minutes, g_state.timer_seconds);
        lv_label_set_text(lbl_timer, buf);
    }

    // Timer play button icon
    lv_obj_t* play_icon = lv_obj_get_child(btn_timer_play, 0);
    if (play_icon) {
        if (g_state.timer_active && !g_state.timer_paused) {
            lv_label_set_text(play_icon, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(play_icon, COLOR_WHITE, 0);
        } else {
            lv_label_set_text(play_icon, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(play_icon, COLOR_GREEN, 0);
        }
    }

    // --- Action button ---
    if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) {
        lv_label_set_text(lbl_action, "ACCENDI");
        lv_obj_set_style_bg_color(btn_action, COLOR_GREEN_DARK, 0);
    } else {
        if (s_hold_hint && millis() - s_hold_hint_start >= HOLD_HINT_MS) s_hold_hint = false;
        lv_label_set_text(lbl_action, s_hold_hint ? "TIENI PREMUTO" : "SPEGNI");
        lv_obj_set_style_bg_color(btn_action, COLOR_RED_DARK, 0);
    }

    // --- Dynamic layout: show/hide boost buttons ---
    bool is_on = g_state.power_on && g_state.mode != OvenMode::STANDBY;
    if (is_on != s_prev_oven_on_layout) {
        s_prev_oven_on_layout = is_on;
        if (is_on) {
            // Compressed layout: SPEGNI 44px + 8px + boost 44px + 6px + MENU 44px
            lv_obj_set_pos(btn_action, 20, 334);
            lv_obj_set_size(btn_action, SCREEN_WIDTH - 40, 44);
            lv_obj_clear_flag(btn_boost_cielo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(btn_boost_platea, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(btn_menu, 20, 436);
            lv_obj_set_size(btn_menu, SCREEN_WIDTH - 40, 44);
        } else {
            // Standard layout: ACCENDI 65px + MENU 50px
            lv_obj_set_pos(btn_action, 20, 340);
            lv_obj_set_size(btn_action, SCREEN_WIDTH - 40, 65);
            lv_obj_add_flag(btn_boost_cielo, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(btn_boost_platea, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(btn_menu, 20, 420);
            lv_obj_set_size(btn_menu, SCREEN_WIDTH - 40, 50);
        }
        // Force refresh boost button display
        s_prev_boost_active_cielo = false;
        s_prev_boost_active_platea = false;
    }

    // --- Boost button visual feedback (no countdown in SEMPLICE) ---
    if (is_on) {
        if (g_state.boost_cielo != s_prev_boost_active_cielo) {
            s_prev_boost_active_cielo = g_state.boost_cielo;
            lv_obj_set_style_bg_color(btn_boost_cielo,
                g_state.boost_cielo ? COLOR_ORANGE : COLOR_BUTTON_BG, 0);
        }
        if (g_state.boost_platea != s_prev_boost_active_platea) {
            s_prev_boost_active_platea = g_state.boost_platea;
            lv_obj_set_style_bg_color(btn_boost_platea,
                g_state.boost_platea ? COLOR_ORANGE : COLOR_BUTTON_BG, 0);
        }
    }
}

// ============================================
// Event Callbacks
// ============================================

static void on_temp_plus_click(lv_event_t* e) {
    buzzer_beep();
    apply_target(s_target_temp + 5);
    oven_state_register_activity();
}

static void on_temp_minus_click(lv_event_t* e) {
    buzzer_beep();
    apply_target(s_target_temp - 5);
    oven_state_register_activity();
}

static void on_target_tap_click(lv_event_t* e) {
    buzzer_beep();
    ui_keypad_show(&s_target_temp, "TEMPERATURA", 50, 500);
    oven_state_register_activity();
}

static void on_timer_tap_click(lv_event_t* e) {
    buzzer_beep();
    ui_keypad_show_time(&g_state.timer_minutes, &g_state.timer_seconds, "TIMER (m:ss)");
    oven_state_register_activity();
}

static void on_timer_play_click(lv_event_t* e) {
    buzzer_beep();
    ui_auto_timer_play();
    oven_state_register_activity();
}

static void on_timer_reset_click(lv_event_t* e) {
    buzzer_beep();
    ui_auto_timer_reset();
    oven_state_register_activity();
}

static bool action_is_armed() {
    return g_state.power_on && g_state.mode != OvenMode::STANDBY;
}

static void action_tap() {
    // ACCENDI -> preheat with single target
    buzzer_beep();
    ui_auto_power_on();
    oven_state_register_activity();
}

static void action_hold() {
    // SPEGNI tenuto premuto -> standby
    buzzer_confirm();
    s_hold_hint = false;
    g_state.power_on = false;
    oven_state_go_standby();
    g_state.timer_active = false;
    g_state.timer_paused = false;
    s_ready_buzzed = false;
    oven_state_register_activity();
    Serial.println("[SEMPLICE] SPEGNI -> STANDBY");
}

static void action_short() {
    buzzer_beep();
    s_hold_hint = true;
    s_hold_hint_start = millis();
}

static void on_boost_cielo_click(lv_event_t* e) {
    buzzer_beep();
    ui_auto_toggle_boost(true);
    oven_state_register_activity();
}

static void on_boost_platea_click(lv_event_t* e) {
    buzzer_beep();
    ui_auto_toggle_boost(false);
    oven_state_register_activity();
}

static void on_menu_click(lv_event_t* e) {
    // Pressione lunga: cambio modalita' gia' richiesto, non aprire il menu
    if (ui_mode_consume_long_press()) return;

    buzzer_beep();
    ui_settings_open();
    oven_state_register_activity();
}
