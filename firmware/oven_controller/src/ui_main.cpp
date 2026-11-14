/**
 * Main UI Screen Implementation - Effeuno style
 * Replicates the exact Effeuno Evolution Multi interface
 */

#include "ui_main.h"
#include "ui_theme.h"
#include "ui_widgets.h"
#include "ui_keypad.h"
#include "ui_settings.h"
#include "ui_programs.h"
#include "ui_mode.h"
#include "oven_state.h"
#include "hardware_pins.h"
#include "wifi_time.h"
#include "buzzer.h"
#include "ssr_control.h"
#include "thermal_profile.h"
#include "cooldown.h"
#include "schedule.h"
#include "programs_storage.h"

// Font custom 72pt per temperatura grande
LV_FONT_DECLARE(lv_font_montserrat_72);

// ============================================
// Static UI elements
// ============================================
static lv_obj_t* screen_main = nullptr;

// Header
static lv_obj_t* lbl_wifi = nullptr;
static lv_obj_t* lbl_datetime = nullptr;
static lv_obj_t* lbl_temp_box = nullptr;
static lv_obj_t* lbl_sound = nullptr;

// Phase indicators (3 circular gauges)
static lv_obj_t* phase_cont[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* phase_arc[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* lbl_phase_temp_top[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* lbl_phase_time[3] = {nullptr, nullptr, nullptr};
static lv_obj_t* lbl_phase_temp_bot[3] = {nullptr, nullptr, nullptr};

// Status bar
static lv_obj_t* bar_status = nullptr;
static lv_obj_t* lbl_prog = nullptr;
static lv_obj_t* lbl_name = nullptr;
static lv_obj_t* lbl_status = nullptr;

// Temperature display
static lv_obj_t* icon_set_cielo = nullptr;
static lv_obj_t* lbl_set_cielo = nullptr;
static lv_obj_t* lbl_temp_big = nullptr;
static lv_obj_t* lbl_temp_platea_small = nullptr;
static lv_obj_t* bar_temp = nullptr;

// Power section
static lv_obj_t* icon_set_platea = nullptr;
static lv_obj_t* lbl_set_platea = nullptr;
static lv_obj_t* lbl_pwr_cielo = nullptr;
static lv_obj_t* lbl_pwr_platea = nullptr;
static lv_obj_t* icon_res_cielo = nullptr;
static lv_obj_t* icon_res_platea = nullptr;

// Controls
static lv_obj_t* btn_restart = nullptr;
static lv_obj_t* btn_play = nullptr;
static lv_obj_t* lbl_timer = nullptr;
static lv_obj_t* btn_save = nullptr;
static lv_timer_t* save_feedback_timer = nullptr;

// Function buttons (2x4 grid)
static lv_obj_t* btn_boost_cielo = nullptr;
static lv_obj_t* btn_preheat = nullptr;
static lv_obj_t* btn_boost_platea = nullptr;
static lv_obj_t* btn_prog = nullptr;
static lv_obj_t* btn_maintain = nullptr;
static lv_obj_t* btn_power = nullptr;
static lv_obj_t* btn_service = nullptr;
static lv_obj_t* btn_menu = nullptr;

// Label change guards (avoid redundant lv_label_set_text at 10Hz)
static int s_prev_minute = -1;
static int s_prev_temp_ambient = -999;
static bool s_prev_sound_enabled = true;
static int s_prev_temp_big = -999;
static int s_prev_target_cielo = -1;
static int s_prev_target_platea = -1;
static int s_prev_timer_min = -1;
static int s_prev_timer_sec = -1;

// Precomputed phase colors (set once in create_phases)
static lv_color_t s_phase_color[3];
static lv_color_t s_phase_color_dim[3];

// ============================================
// Forward declarations
// ============================================
static void create_header(lv_obj_t* parent);
static void create_phases(lv_obj_t* parent);
static void create_status_bar(lv_obj_t* parent);
static void create_temp_display(lv_obj_t* parent);
static void create_power_section(lv_obj_t* parent);
static void create_controls(lv_obj_t* parent);
static void create_func_buttons(lv_obj_t* parent);

// Event handlers
static void on_phase_click(lv_event_t* e);
static void on_phase_long_press(lv_event_t* e);
static void on_save_feedback_timer(lv_timer_t* timer);
static void on_set_cielo_click(lv_event_t* e);
static void on_set_platea_click(lv_event_t* e);
static void on_pwr_cielo_click(lv_event_t* e);
static void on_pwr_platea_click(lv_event_t* e);
static void on_restart_click(lv_event_t* e);
static void on_play_click(lv_event_t* e);
static void on_timer_click(lv_event_t* e);
static void on_save_click(lv_event_t* e);
static void on_boost_cielo_click(lv_event_t* e);
static void on_preheat_click(lv_event_t* e);
static void on_boost_platea_click(lv_event_t* e);
static void on_maintain_click(lv_event_t* e);
static void on_power_click(lv_event_t* e);

// POWER: accensione con un tocco, spegnimento tenendo premuto 1s
#define HOLD_HINT_MS 1500
static bool s_power_hint = false;
static unsigned long s_power_hint_start = 0;
static bool power_is_armed();
static void power_tap();
static void power_hold();
static void power_short();
static HoldButton s_power_hold = {power_is_armed, power_tap, power_hold, power_short,
                                  nullptr, 0, false, false};
static void on_service_click(lv_event_t* e);
static void on_prog_click(lv_event_t* e);
static void on_menu_click(lv_event_t* e);

// ============================================
// Create main screen
// ============================================
void ui_main_create() {
    // Elimina un eventuale schermo PRO precedente non piu' attivo (evita leak
    // a ogni cambio modalita')
    ui_main_destroy();

    // Reset label guards to force full refresh on new screen
    s_prev_minute = -1;
    s_prev_temp_ambient = -999;
    s_prev_sound_enabled = !buzzer_is_enabled();
    s_prev_temp_big = -999;
    s_prev_target_cielo = -1;
    s_prev_target_platea = -1;
    s_prev_timer_min = -1;
    s_prev_timer_sec = -1;

    screen_main = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen_main, COLOR_BG, 0);
    lv_obj_clear_flag(screen_main, LV_OBJ_FLAG_SCROLLABLE);

    create_header(screen_main);
    create_phases(screen_main);
    create_status_bar(screen_main);
    create_temp_display(screen_main);
    create_power_section(screen_main);
    create_controls(screen_main);
    create_func_buttons(screen_main);

    lv_scr_load(screen_main);
    Serial.println("Main UI created (Effeuno style)");
}

// ============================================
// Header Section
// ============================================
static void create_header(lv_obj_t* parent) {
    // Header container
    lv_obj_t* header = lv_obj_create(parent);
    lv_obj_set_size(header, SCREEN_WIDTH, HEADER_HEIGHT);
    lv_obj_set_pos(header, 0, HEADER_Y);
    lv_obj_set_style_bg_color(header, COLOR_BG, 0);
    lv_obj_set_style_radius(header, 0, 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_set_style_pad_all(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi icon (left side) - inizia nascosta
    lbl_wifi = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_wifi, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_wifi, COLOR_GRAY, 0);
    lv_label_set_text(lbl_wifi, LV_SYMBOL_WIFI);
    lv_obj_set_pos(lbl_wifi, 8, (HEADER_HEIGHT - 14) / 2);  // Posizione fissa

    // Date/time label - posizione fissa, inizia vuota
    lbl_datetime = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_datetime, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_datetime, COLOR_WHITE, 0);
    lv_label_set_text(lbl_datetime, "");  // Inizia vuota
    lv_obj_set_pos(lbl_datetime, 10, (HEADER_HEIGHT - 14) / 2);  // Stessa posizione dell'icona

    // Box temperature (left of sound icon)
    lbl_temp_box = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_temp_box, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_temp_box, COLOR_WHITE, 0);
    lv_label_set_text(lbl_temp_box, "");
    // Allineato a destra (prima dell'altoparlante): segue la larghezza del testo
    lv_obj_align(lbl_temp_box, LV_ALIGN_RIGHT_MID, -32, 0);

    // Sound icon (right side)
    lbl_sound = lv_label_create(header);
    lv_obj_set_style_text_font(lbl_sound, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_sound, COLOR_WHITE, 0);
    lv_label_set_text(lbl_sound, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_pos(lbl_sound, SCREEN_WIDTH - 25, (HEADER_HEIGHT - 14) / 2);
}

// ============================================
// Phase Gauges Section
// ============================================
static void create_phases(lv_obj_t* parent) {
    // Container for phases
    lv_obj_t* phases = lv_obj_create(parent);
    lv_obj_set_size(phases, SCREEN_WIDTH, PHASES_HEIGHT);
    lv_obj_set_pos(phases, 0, PHASES_Y);
    lv_obj_set_style_bg_color(phases, COLOR_BG, 0);
    lv_obj_set_style_radius(phases, 0, 0);
    lv_obj_set_style_border_width(phases, 0, 0);
    lv_obj_set_style_pad_all(phases, 0, 0);
    lv_obj_clear_flag(phases, LV_OBJ_FLAG_SCROLLABLE);

    // Create 3 phase indicators
    int cx[3] = {53, 160, 267};  // Center X positions
    int cy = 52;  // Center Y position

    for (int i = 0; i < 3; i++) {
        // Container for each phase
        phase_cont[i] = lv_obj_create(phases);
        lv_obj_set_size(phase_cont[i], GAUGE_SIZE, GAUGE_SIZE);
        lv_obj_set_pos(phase_cont[i], cx[i] - GAUGE_SIZE/2, cy - GAUGE_SIZE/2);
        lv_obj_set_style_bg_opa(phase_cont[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(phase_cont[i], 0, 0);
        lv_obj_set_style_pad_all(phase_cont[i], 0, 0);
        lv_obj_clear_flag(phase_cont[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(phase_cont[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(phase_cont[i], on_phase_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        lv_obj_add_event_cb(phase_cont[i], on_phase_long_press, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);

        // Arc (gauge background with ticks)
        phase_arc[i] = lv_arc_create(phase_cont[i]);
        lv_obj_set_size(phase_arc[i], GAUGE_SIZE - 8, GAUGE_SIZE - 8);
        lv_obj_center(phase_arc[i]);
        lv_arc_set_rotation(phase_arc[i], 135);
        lv_arc_set_bg_angles(phase_arc[i], 0, 270);
        lv_arc_set_range(phase_arc[i], 0, 100);
        lv_arc_set_value(phase_arc[i], 0);
        // Colori fase: Fase 1 giallo, Fase 2 arancione, Fase 3 rosso
        lv_color_t phase_color = (i == 0) ? COLOR_YELLOW : (i == 1) ? COLOR_ORANGE : COLOR_RED;
        // Sfondo arco: colore fase "spento" (mescolato con nero al 25%)
        lv_color_t phase_color_dim = lv_color_mix(phase_color, lv_color_black(), 64);
        lv_obj_set_style_arc_color(phase_arc[i], phase_color_dim, LV_PART_MAIN);
        lv_obj_set_style_arc_width(phase_arc[i], 6, LV_PART_MAIN);
        // Indicatore: colore fase pieno
        lv_obj_set_style_arc_color(phase_arc[i], phase_color, LV_PART_INDICATOR);
        lv_obj_set_style_arc_width(phase_arc[i], 6, LV_PART_INDICATOR);
        lv_obj_remove_style(phase_arc[i], NULL, LV_PART_KNOB);
        lv_obj_clear_flag(phase_arc[i], LV_OBJ_FLAG_CLICKABLE);

        // Top temperature label (above center)
        lbl_phase_temp_top[i] = lv_label_create(phase_cont[i]);
        lv_obj_set_style_text_font(lbl_phase_temp_top[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_phase_temp_top[i], COLOR_WHITE, 0);
        lv_label_set_text(lbl_phase_temp_top[i], "155");
        lv_obj_align(lbl_phase_temp_top[i], LV_ALIGN_CENTER, 0, -18);

        // Time label (center, colore della fase: giallo/arancione/rosso)
        lbl_phase_time[i] = lv_label_create(phase_cont[i]);
        lv_obj_set_style_text_font(lbl_phase_time[i], &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl_phase_time[i], phase_color, 0);
        lv_label_set_text(lbl_phase_time[i], "2:00");
        lv_obj_align(lbl_phase_time[i], LV_ALIGN_CENTER, 0, 2);

        // Bottom temperature label (below center)
        lbl_phase_temp_bot[i] = lv_label_create(phase_cont[i]);
        lv_obj_set_style_text_font(lbl_phase_temp_bot[i], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lbl_phase_temp_bot[i], COLOR_WHITE, 0);
        lv_label_set_text(lbl_phase_temp_bot[i], "155");
        lv_obj_align(lbl_phase_temp_bot[i], LV_ALIGN_CENTER, 0, 22);
    }

    // Precompute phase colors
    s_phase_color[0] = COLOR_YELLOW; s_phase_color[1] = COLOR_ORANGE; s_phase_color[2] = COLOR_RED;
    s_phase_color_dim[0] = lv_color_mix(COLOR_YELLOW, lv_color_black(), 64);
    s_phase_color_dim[1] = lv_color_mix(COLOR_ORANGE, lv_color_black(), 64);
    s_phase_color_dim[2] = lv_color_mix(COLOR_RED, lv_color_black(), 64);
}

// ============================================
// Status Bar Section
// ============================================
static void create_status_bar(lv_obj_t* parent) {
    bar_status = lv_obj_create(parent);
    lv_obj_set_size(bar_status, SCREEN_WIDTH, STATUS_HEIGHT);
    lv_obj_set_pos(bar_status, 0, STATUS_Y);
    lv_obj_set_style_bg_color(bar_status, COLOR_BG, 0);
    lv_obj_set_style_radius(bar_status, 0, 0);
    lv_obj_set_style_border_width(bar_status, 0, 0);
    lv_obj_set_style_border_color(bar_status, COLOR_DARK_GRAY, 0);
    lv_obj_set_style_border_side(bar_status, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(bar_status, 0, 0);
    lv_obj_clear_flag(bar_status, LV_OBJ_FLAG_SCROLLABLE);

    // PROG label
    lbl_prog = lv_label_create(bar_status);
    lv_obj_set_style_text_font(lbl_prog, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_prog, COLOR_WHITE, 0);
    lv_label_set_text(lbl_prog, "P0");
    lv_obj_set_width(lbl_prog, 112);
    lv_label_set_long_mode(lbl_prog, LV_LABEL_LONG_DOT);
    lv_obj_align(lbl_prog, LV_ALIGN_LEFT_MID, 10, 0);

    // Countdown / program name (bold, larger font)
    lbl_name = lv_label_create(bar_status);
    lv_obj_set_style_text_font(lbl_name, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_name, COLOR_WHITE, 0);
    lv_label_set_text(lbl_name, "PYRO");
    lv_obj_align(lbl_name, LV_ALIGN_CENTER, 0, 0);

    // Status text
    lbl_status = lv_label_create(bar_status);
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_status, COLOR_WHITE, 0);
    lv_label_set_text(lbl_status, "STANDBY");
    lv_obj_align(lbl_status, LV_ALIGN_RIGHT_MID, -10, 0);
}

// ============================================
// Temperature & Power Section (layout 3 colonne stile Effeuno)
// ============================================
static void create_temp_display(lv_obj_t* parent) {
    // Container unico per temperatura e potenza
    lv_obj_t* main_cont = lv_obj_create(parent);
    lv_obj_set_size(main_cont, SCREEN_WIDTH, TEMP_HEIGHT + SETPWR_HEIGHT);
    lv_obj_set_pos(main_cont, 0, TEMP_Y);
    lv_obj_set_style_bg_color(main_cont, COLOR_BG, 0);
    lv_obj_set_style_radius(main_cont, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_style_pad_all(main_cont, 0, 0);
    lv_obj_clear_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);

    // Coordinate colonne
    int col1_x = 30;    // Colonna 1 (SET) - centro
    int col2_x = 145;   // Colonna 2 (Temp/Power) - centro
    int col3_x = 245;   // Colonna 3 (°C, platea, barra) - centro

    // ==========================================
    // COLONNA 1: SET cielo e platea
    // ==========================================

    // --- ROW 1: SET CIELO (bottone unico: SET + icona + temperatura) ---
    lv_obj_t* btn_set_cielo = lv_obj_create(main_cont);
    lv_obj_set_size(btn_set_cielo, 60, 64);
    lv_obj_set_pos(btn_set_cielo, col1_x - 20, 0);
    lv_obj_set_style_bg_opa(btn_set_cielo, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_set_cielo, 0, 0);
    lv_obj_set_style_pad_all(btn_set_cielo, 0, 0);
    lv_obj_clear_flag(btn_set_cielo, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn_set_cielo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_set_cielo, on_set_cielo_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_set_txt1 = lv_label_create(btn_set_cielo);
    lv_obj_set_style_text_font(lbl_set_txt1, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_set_txt1, COLOR_WHITE, 0);
    lv_label_set_text(lbl_set_txt1, "SET");
    lv_obj_set_pos(lbl_set_txt1, 8, 2);

    icon_set_cielo = ui_widget_create_set_icon_cielo(btn_set_cielo);
    lv_obj_set_pos(icon_set_cielo, 5, 18);

    lbl_set_cielo = lv_label_create(btn_set_cielo);
    lv_obj_set_style_text_font(lbl_set_cielo, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_set_cielo, COLOR_WHITE, 0);
    lv_label_set_text(lbl_set_cielo, "155°");
    lv_obj_set_pos(lbl_set_cielo, 2, 42);

    // --- ROW 2: SET PLATEA (bottone unico: SET + icona + temperatura) ---
    lv_obj_t* btn_set_platea = lv_obj_create(main_cont);
    lv_obj_set_size(btn_set_platea, 60, 64);
    lv_obj_set_pos(btn_set_platea, col1_x - 20, 73);
    lv_obj_set_style_bg_opa(btn_set_platea, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_set_platea, 0, 0);
    lv_obj_set_style_pad_all(btn_set_platea, 0, 0);
    lv_obj_clear_flag(btn_set_platea, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(btn_set_platea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn_set_platea, on_set_platea_click, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_set_txt2 = lv_label_create(btn_set_platea);
    lv_obj_set_style_text_font(lbl_set_txt2, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_set_txt2, COLOR_WHITE, 0);
    lv_label_set_text(lbl_set_txt2, "SET");
    lv_obj_set_pos(lbl_set_txt2, 8, 2);

    icon_set_platea = ui_widget_create_set_icon_platea(btn_set_platea);
    lv_obj_set_pos(icon_set_platea, 5, 18);

    lbl_set_platea = lv_label_create(btn_set_platea);
    lv_obj_set_style_text_font(lbl_set_platea, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_set_platea, COLOR_WHITE, 0);
    lv_label_set_text(lbl_set_platea, "155°");
    lv_obj_set_pos(lbl_set_platea, 2, 42);

    // ==========================================
    // COLONNA 2: Temperatura grande e potenze (centrata)
    // ==========================================

    // Temperatura cielo grande (font 72) - centrata
    lbl_temp_big = lv_label_create(main_cont);
    lv_obj_set_style_text_font(lbl_temp_big, &lv_font_montserrat_72, 0);
    lv_obj_set_style_text_color(lbl_temp_big, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(lbl_temp_big, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_temp_big, 150);
    lv_label_set_text(lbl_temp_big, "104");
    lv_obj_set_pos(lbl_temp_big, col2_x - 75, 0);

    // Potenza cielo % - centrata
    lbl_pwr_cielo = lv_label_create(main_cont);
    lv_obj_set_style_text_font(lbl_pwr_cielo, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_pwr_cielo, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(lbl_pwr_cielo, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_pwr_cielo, 80);
    lv_label_set_text(lbl_pwr_cielo, "75 %");
    lv_obj_set_pos(lbl_pwr_cielo, col2_x - 40, 70);
    lv_obj_add_flag(lbl_pwr_cielo, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_pwr_cielo, on_pwr_cielo_click, LV_EVENT_CLICKED, NULL);

    // Linea separatrice orizzontale - centrata
    lv_obj_t* line_sep = lv_obj_create(main_cont);
    lv_obj_set_size(line_sep, 80, 2);
    lv_obj_set_pos(line_sep, col2_x - 40, 100);
    lv_obj_set_style_bg_color(line_sep, COLOR_GRAY, 0);
    lv_obj_set_style_radius(line_sep, 0, 0);
    lv_obj_set_style_border_width(line_sep, 0, 0);

    // Potenza platea % - centrata
    lbl_pwr_platea = lv_label_create(main_cont);
    lv_obj_set_style_text_font(lbl_pwr_platea, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_pwr_platea, COLOR_WHITE, 0);
    lv_obj_set_style_text_align(lbl_pwr_platea, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(lbl_pwr_platea, 80);
    lv_label_set_text(lbl_pwr_platea, "75 %");
    lv_obj_set_pos(lbl_pwr_platea, col2_x - 40, 108);
    lv_obj_add_flag(lbl_pwr_platea, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(lbl_pwr_platea, on_pwr_platea_click, LV_EVENT_CLICKED, NULL);

    // ==========================================
    // COLONNA 3: °C, simbolo platea, temp platea, res cielo, res platea
    // ==========================================

    // 1. °C in alto
    lv_obj_t* lbl_degC = lv_label_create(main_cont);
    lv_obj_set_style_text_font(lbl_degC, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_degC, COLOR_WHITE, 0);
    lv_label_set_text(lbl_degC, "\xC2\xB0" "C");
    lv_obj_set_pos(lbl_degC, col3_x - 5, 2);

    // 2. Simbolo platea
    lv_obj_t* icon_platea_small = ui_widget_create_set_icon_platea(main_cont);
    lv_obj_set_pos(icon_platea_small, col3_x - 10, 26);

    // 3. Temperatura platea con ° (senza C)
    lbl_temp_platea_small = lv_label_create(main_cont);
    lv_obj_set_style_text_font(lbl_temp_platea_small, &lv_font_montserrat_18, 0);
    lv_obj_set_style_text_color(lbl_temp_platea_small, COLOR_WHITE, 0);
    lv_label_set_text(lbl_temp_platea_small, "47\xC2\xB0");
    lv_obj_set_pos(lbl_temp_platea_small, col3_x - 10, 50);

    // 4. Icona resistenza cielo (40x20)
    icon_res_cielo = ui_widget_create_resistance_icon(main_cont);
    lv_obj_set_pos(icon_res_cielo, col3_x - 20, 75);

    // 5. Icona resistenza platea (40x20)
    icon_res_platea = ui_widget_create_resistance_icon(main_cont);
    lv_obj_set_pos(icon_res_platea, col3_x - 20, 110);

    // ==========================================
    // COLONNA 4: Barra temperatura (alta tutta la sezione)
    // ==========================================
    bar_temp = ui_widget_create_temp_bar(main_cont);
    lv_obj_set_pos(bar_temp, SCREEN_WIDTH - 28, 2);
}

// ============================================
// Power Section - ora vuota (integrata in create_temp_display)
// ============================================
static void create_power_section(lv_obj_t* parent) {
    (void)parent;  // Tutto integrato sopra
}

// ============================================
// Controls Section
// ============================================
static void create_controls(lv_obj_t* parent) {
    lv_obj_t* ctrl_cont = lv_obj_create(parent);
    lv_obj_set_size(ctrl_cont, SCREEN_WIDTH, CONTROLS_HEIGHT);
    lv_obj_set_pos(ctrl_cont, 0, CONTROLS_Y);
    lv_obj_set_style_bg_color(ctrl_cont, COLOR_BG, 0);
    lv_obj_set_style_radius(ctrl_cont, 0, 0);
    lv_obj_set_style_border_width(ctrl_cont, 0, 0);
    lv_obj_set_style_pad_all(ctrl_cont, 0, 0);
    lv_obj_clear_flag(ctrl_cont, LV_OBJ_FLAG_SCROLLABLE);

    int y = 3;

    // Restart button (orange)
    btn_restart = ui_widget_create_ctrl_btn(ctrl_cont, CtrlBtnType::RESTART);
    lv_obj_set_pos(btn_restart, 10, y);
    lv_obj_add_event_cb(btn_restart, on_restart_click, LV_EVENT_CLICKED, NULL);

    // Play button (green)
    btn_play = ui_widget_create_ctrl_btn(ctrl_cont, CtrlBtnType::PLAY);
    lv_obj_set_pos(btn_play, 70, y);
    lv_obj_add_event_cb(btn_play, on_play_click, LV_EVENT_CLICKED, NULL);

    // Timer display (clickable)
    lv_obj_t* timer_btn = lv_btn_create(ctrl_cont);
    lv_obj_set_size(timer_btn, 90, CTRL_BTN_SIZE);
    lv_obj_set_pos(timer_btn, 130, y);
    lv_obj_set_style_bg_color(timer_btn, COLOR_BUTTON_BG, 0);
    lv_obj_set_style_radius(timer_btn, 6, 0);
    lv_obj_set_style_border_width(timer_btn, 1, 0);
    lv_obj_set_style_border_color(timer_btn, COLOR_GRAY, 0);
    lv_obj_add_event_cb(timer_btn, on_timer_click, LV_EVENT_CLICKED, NULL);

    lbl_timer = lv_label_create(timer_btn);
    lv_obj_set_style_text_font(lbl_timer, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_timer, COLOR_WHITE, 0);
    lv_label_set_text(lbl_timer, "0:00");
    lv_obj_center(lbl_timer);

    // Save button
    btn_save = ui_widget_create_ctrl_btn(ctrl_cont, CtrlBtnType::SAVE);
    lv_obj_set_pos(btn_save, SCREEN_WIDTH - 60, y);
    lv_obj_add_event_cb(btn_save, on_save_click, LV_EVENT_CLICKED, NULL);
}

// ============================================
// Function Buttons Section (2x4 grid)
// ============================================
static void create_func_buttons(lv_obj_t* parent) {
    lv_obj_t* btn_cont = lv_obj_create(parent);
    lv_obj_set_size(btn_cont, SCREEN_WIDTH, BUTTONS_HEIGHT);
    lv_obj_set_pos(btn_cont, 0, BUTTONS_Y);
    lv_obj_set_style_bg_color(btn_cont, COLOR_BG, 0);  // Sfondo nero come il resto
    lv_obj_set_style_radius(btn_cont, 0, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);
    lv_obj_clear_flag(btn_cont, LV_OBJ_FLAG_SCROLLABLE);

    int btnW = FUNC_BTN_WIDTH;
    int btnH = FUNC_BTN_HEIGHT;
    int spacing = FUNC_BTN_SPACING;
    int startX = 8;
    int y1 = 6;
    int y2 = y1 + btnH + spacing;

    // Row 1: Preheat, Maintain, Boost Cielo, Boost Platea
    btn_preheat = ui_widget_create_func_btn(btn_cont, FuncBtnType::PREHEAT);
    lv_obj_set_pos(btn_preheat, startX, y1);
    lv_obj_add_event_cb(btn_preheat, on_preheat_click, LV_EVENT_CLICKED, NULL);

    btn_maintain = ui_widget_create_func_btn(btn_cont, FuncBtnType::MAINTAIN);
    lv_obj_set_pos(btn_maintain, startX + (btnW + spacing), y1);
    lv_obj_add_event_cb(btn_maintain, on_maintain_click, LV_EVENT_CLICKED, NULL);

    btn_boost_cielo = ui_widget_create_func_btn(btn_cont, FuncBtnType::BOOST_CIELO);
    lv_obj_set_pos(btn_boost_cielo, startX + 2*(btnW + spacing), y1);
    lv_obj_add_event_cb(btn_boost_cielo, on_boost_cielo_click, LV_EVENT_CLICKED, NULL);

    btn_boost_platea = ui_widget_create_func_btn(btn_cont, FuncBtnType::BOOST_PLATEA);
    lv_obj_set_pos(btn_boost_platea, startX + 3*(btnW + spacing), y1);
    lv_obj_add_event_cb(btn_boost_platea, on_boost_platea_click, LV_EVENT_CLICKED, NULL);

    // Row 2: Power, Servizio pizze, Programs, Menu
    btn_power = ui_widget_create_func_btn(btn_cont, FuncBtnType::POWER);
    lv_obj_set_pos(btn_power, startX, y2);
    ui_widget_attach_hold(btn_power, &s_power_hold);

    btn_service = ui_widget_create_func_btn(btn_cont, FuncBtnType::SERVICE);
    lv_obj_set_pos(btn_service, startX + (btnW + spacing), y2);
    lv_obj_add_event_cb(btn_service, on_service_click, LV_EVENT_CLICKED, NULL);

    btn_prog = ui_widget_create_func_btn(btn_cont, FuncBtnType::PROGRAMS);
    lv_obj_set_pos(btn_prog, startX + 2*(btnW + spacing), y2);
    lv_obj_add_event_cb(btn_prog, on_prog_click, LV_EVENT_CLICKED, NULL);

    btn_menu = ui_widget_create_func_btn(btn_cont, FuncBtnType::MENU);
    lv_obj_set_pos(btn_menu, startX + 3*(btnW + spacing), y2);
    lv_obj_add_event_cb(btn_menu, on_menu_click, LV_EVENT_CLICKED, NULL);
    ui_mode_attach_menu_long_press(btn_menu);  // hold 1s -> passa a SEMPLICE
}

// ============================================
// Update UI from state
// ============================================
void ui_main_update() {
    char buf[32];

    // Update WiFi icon e data/ora
    WiFiStatus wifi_status = wifi_time_get_status();
    bool time_synced = wifi_time_is_synced();

    // Icona WiFi indica stato connessione reale
    bool wifi_icon_visible = false;
    if (wifi_status == WiFiStatus::CONNECTING) {
        // In connessione - lampeggia arancione
        lv_obj_set_style_text_color(lbl_wifi,
            g_state.resistance_blink_on ? COLOR_ORANGE : COLOR_GRAY, 0);
        lv_obj_clear_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
        wifi_icon_visible = true;
    }
    else if (wifi_status == WiFiStatus::CONNECTED) {
        // Connesso - verde
        lv_obj_set_style_text_color(lbl_wifi, COLOR_GREEN, 0);
        lv_obj_clear_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
        wifi_icon_visible = true;
    }
    else {
        // Disconnesso o fallito - nascondi icona
        lv_obj_add_flag(lbl_wifi, LV_OBJ_FLAG_HIDDEN);
    }

    // Data/ora visibile solo se sincronizzata
    if (time_synced) {
        if (g_state.minute != s_prev_minute) {
            s_prev_minute = g_state.minute;
            sprintf(buf, "%s %02d.%02d.%04d - %02d:%02d",
                    oven_state_get_day_name(g_state.day_of_week),
                    g_state.day, g_state.month, g_state.year,
                    g_state.hour, g_state.minute);
            lv_label_set_text(lbl_datetime, buf);
        }
        // Posizione: dopo icona WiFi se visibile, altrimenti all'inizio
        lv_obj_set_pos(lbl_datetime, wifi_icon_visible ? 30 : 10, (HEADER_HEIGHT - 14) / 2);
    }
    else {
        lv_label_set_text(lbl_datetime, "");
        s_prev_minute = -1;
    }

    // Update box temperature (cold junction / ambient)
    if (g_state.temp_ambient != s_prev_temp_ambient) {
        s_prev_temp_ambient = g_state.temp_ambient;
        char box_buf[12];
        // "BOX": e' la temperatura dell'elettronica, non del forno
        snprintf(box_buf, sizeof(box_buf), "BOX %d°", g_state.temp_ambient);
        lv_label_set_text(lbl_temp_box, box_buf);
    }

    // Update sound icon (right side of header)
    bool sound_on = buzzer_is_enabled();
    if (sound_on != s_prev_sound_enabled) {
        s_prev_sound_enabled = sound_on;
        lv_label_set_text(lbl_sound, sound_on ? LV_SYMBOL_VOLUME_MAX : LV_SYMBOL_MUTE);
    }
    lv_obj_set_style_text_color(lbl_sound, COLOR_WHITE, 0);

    // Update phase indicators
    for (int i = 0; i < 3; i++) {
        bool used = (i < g_state.num_phases);
        bool active = (i == g_state.active_phase);

        lv_color_t phase_color = s_phase_color[i];
        lv_color_t phase_color_dim = s_phase_color_dim[i];

        // Fase non usata: arco sbiadito e senza testi (prima "--" e "--:--")
        lv_opa_t arc_opa = used ? LV_OPA_COVER : LV_OPA_30;
        if (lv_obj_get_style_opa(phase_arc[i], 0) != arc_opa) {
            lv_obj_set_style_opa(phase_arc[i], arc_opa, 0);
        }

        if (used) {
            sprintf(buf, "%d\xC2\xB0", g_state.phases[i].temp_cielo);
            lv_label_set_text(lbl_phase_temp_top[i], buf);

            // La fase attiva mostra il timer (anche 1:30): prima mostrava la
            // durata salvata, e dopo aver cambiato il timer i due non coincidevano
            if (active) {
                sprintf(buf, "%d:%02d", g_state.timer_minutes, g_state.timer_seconds);
            } else {
                sprintf(buf, "%d:00", g_state.phases[i].duration);
            }
            lv_label_set_text(lbl_phase_time[i], buf);

            sprintf(buf, "%d\xC2\xB0", g_state.phases[i].temp_platea);
            lv_label_set_text(lbl_phase_temp_bot[i], buf);

            // Set colors - tempo usa il colore della fase quando attiva
            // Testi bianchi anche per le fasi non attive: il grigio sul display
            // si leggeva male (lo stato lo dice gia' il colore dell'arco)
            lv_color_t text_col = COLOR_WHITE;
            lv_obj_set_style_text_color(lbl_phase_temp_top[i], text_col, 0);
            lv_obj_set_style_text_color(lbl_phase_temp_bot[i], text_col, 0);
            lv_obj_set_style_text_color(lbl_phase_time[i], active ? phase_color : COLOR_WHITE, 0);

            // Arc: sfondo sempre colore fase spento, indicatore colore fase pieno
            lv_obj_set_style_arc_color(phase_arc[i], phase_color_dim, LV_PART_MAIN);
            lv_obj_set_style_arc_color(phase_arc[i], phase_color, LV_PART_INDICATOR);

            // Calcola progresso arco
            int progress = 0;
            if (i < g_state.active_phase) {
                // Fase completata
                progress = 100;
            } else if (active && g_state.timer_active && g_state.phases[i].duration > 0) {
                // Fase attiva con timer in corso
                int total_seconds = g_state.phases[i].duration * 60;
                int remaining_seconds = g_state.timer_minutes * 60 + g_state.timer_seconds;
                int elapsed_seconds = total_seconds - remaining_seconds;
                if (elapsed_seconds < 0) elapsed_seconds = 0;
                if (elapsed_seconds > total_seconds) elapsed_seconds = total_seconds;
                progress = (elapsed_seconds * 100) / total_seconds;
            }
            // Fase futura: progress rimane 0

            lv_arc_set_value(phase_arc[i], progress);
        } else {
            lv_label_set_text(lbl_phase_temp_top[i], "");
            lv_label_set_text(lbl_phase_time[i], "");
            lv_label_set_text(lbl_phase_temp_bot[i], "");
            lv_obj_set_style_text_color(lbl_phase_temp_top[i], COLOR_DARK_GRAY, 0);
            lv_obj_set_style_text_color(lbl_phase_time[i], COLOR_DARK_GRAY, 0);
            lv_obj_set_style_text_color(lbl_phase_temp_bot[i], COLOR_DARK_GRAY, 0);
            // Fase non usata: sfondo colore spento, nessun progresso
            lv_obj_set_style_arc_color(phase_arc[i], phase_color_dim, LV_PART_MAIN);
            lv_arc_set_value(phase_arc[i], 0);
        }
    }

    // Update status bar: "P0 MANUALE" dice di piu' di "PROG. 0"
    snprintf(buf, sizeof(buf), "P%d %s", g_state.program_number, g_state.program_name);
    if (strcmp(lv_label_get_text(lbl_prog), buf) != 0) lv_label_set_text(lbl_prog, buf);

    // Countdown al forno realmente pronto: salita delle sonde + saturazione
    // della pietra (la sonda platea sta sotto la pietra, non sulla superficie)
    bool countdown_active = false;
    bool countdown_ready = false;
    bool countdown_cooling = false;
    if (g_state.power_on) {
        if (g_state.error_cielo || g_state.error_platea) {
            lv_label_set_text(lbl_name, g_state.error_cielo ? "SONDA C" : "SONDA P");
        } else if (g_state.mode == OvenMode::STANDBY) {
            // Acceso ma fermo: nessun ETA da mostrare (prima "-0m")
            lv_label_set_text(lbl_name, "-- : --");
        } else if (g_state.recovery_active) {
            lv_label_set_text(lbl_name, "RECUPERO");
            countdown_active = true;
        } else if (g_state.stone_ready) {
            // Pietra carica, ma dopo un'infornata la camera puo' essere molto
            // sotto il setpoint: in quel caso si mostra quanto manca davvero
            int deficit = 0, eta_back = 0;
            if (oven_state_temp_deficit(&deficit, &eta_back)) {
                sprintf(buf, "-%d\xC2\xB0 ~%dm", deficit, eta_back);
                lv_label_set_text(lbl_name, buf);
                countdown_active = true;
            } else {
                lv_label_set_text(lbl_name, "PRONTO");
                countdown_ready = true;
            }
        } else {
            int eta = thermal_profile_get_ready_eta_minutes();
            if (eta >= 60) {
                sprintf(buf, "-%dh%02dm", eta / 60, eta % 60);
            } else {
                sprintf(buf, "-%dm", eta);
            }
            lv_label_set_text(lbl_name, buf);
            countdown_active = true;
        }
    } else {
        // Spento: quanto manca a scendere sotto i 40 gradi
        int cool = cooldown_get_minutes();
        if (schedule_get_state() == ScheduleState::ARMED) {
            // Si accendera' da solo: prima del raffreddamento
            sprintf(buf, "AVVIO %02d:%02d", schedule_get_hour(), schedule_get_minute());
            lv_label_set_text(lbl_name, buf);
            countdown_cooling = true;  // stesso colore arancio
        } else if (cool > 0) {
            if (cool >= 60) {
                sprintf(buf, "%dÂ° ~%dh%02dm", COOL_TARGET_C, cool / 60, cool % 60);
            } else {
                sprintf(buf, "%dÂ° ~%dm", COOL_TARGET_C, cool);
            }
            lv_label_set_text(lbl_name, buf);
            countdown_cooling = true;
        } else {
            lv_label_set_text(lbl_name, "-- : --");
        }
    }

    if (g_state.error_fan) {
        // Stesso arresto per ventola ferma e scheda surriscaldata
        lv_label_set_text(lbl_status, g_state.error_msg_fan[0] == 'B' ? "BOX HOT" : "FAN ERR");
        lv_obj_set_style_bg_color(bar_status, COLOR_RED, 0);
        lv_obj_set_style_text_color(lbl_prog, COLOR_WHITE, 0);
        lv_obj_set_style_text_color(lbl_name, COLOR_WHITE, 0);
        lv_obj_set_style_text_color(lbl_status, COLOR_WHITE, 0);
    } else {
        if (s_power_hint && millis() - s_power_hint_start >= HOLD_HINT_MS) s_power_hint = false;
        lv_label_set_text(lbl_status, s_power_hint ? "TIENI 1s" : oven_state_get_mode_string());

        // Status bar background color based on mode
        ui_widget_update_status_bar(bar_status, g_state.mode, g_state.timer_expired, g_state.timer_blink);

        // Status bar text color: nero su sfondo chiaro, bianco su sfondo scuro
        lv_color_t status_text_color = COLOR_WHITE;
        if (g_state.mode == OvenMode::READY ||
            g_state.mode == OvenMode::PREHEAT ||
            g_state.mode == OvenMode::MAINTAIN) {
            status_text_color = lv_color_black();
        }
        lv_obj_set_style_text_color(lbl_prog, status_text_color, 0);
        lv_obj_set_style_text_color(lbl_status, status_text_color, 0);

        // Countdown: colore contrastato fisso (giallo = riscaldamento, verde = pronto)
        if (countdown_active) {
            lv_obj_set_style_text_color(lbl_name, COLOR_YELLOW, 0);
        } else if (countdown_ready) {
            lv_obj_set_style_text_color(lbl_name, COLOR_GREEN, 0);
        } else if (countdown_cooling) {
            lv_obj_set_style_text_color(lbl_name, COLOR_ORANGE, 0);
        } else {
            lv_obj_set_style_text_color(lbl_name, status_text_color, 0);
        }
    }

    // Update big temperature (senza °, sta nella colonna 3)
    {
        int temp_big_val = g_state.error_cielo ? -1000 : g_state.temp_cielo;
        if (temp_big_val != s_prev_temp_big) {
            s_prev_temp_big = temp_big_val;
            if (temp_big_val == -1000) {
                lv_label_set_text(lbl_temp_big, "ERR");
                lv_obj_set_style_text_color(lbl_temp_big, COLOR_RED, 0);
            } else {
                sprintf(buf, "%d", temp_big_val);
                lv_label_set_text(lbl_temp_big, buf);
                lv_obj_set_style_text_color(lbl_temp_big, COLOR_WHITE, 0);
            }
        }
    }

    // Update SET values
    if (g_state.target_cielo != s_prev_target_cielo) {
        s_prev_target_cielo = g_state.target_cielo;
        sprintf(buf, "%d\xC2\xB0", g_state.target_cielo);
        lv_label_set_text(lbl_set_cielo, buf);
    }

    if (g_state.target_platea != s_prev_target_platea) {
        s_prev_target_platea = g_state.target_platea;
        sprintf(buf, "%d\xC2\xB0", g_state.target_platea);
        lv_label_set_text(lbl_set_platea, buf);
    }

    // Update platea temperature small (solo ° senza C)
    if (g_state.error_platea) {
        lv_label_set_text(lbl_temp_platea_small, "ERR");
        lv_obj_set_style_text_color(lbl_temp_platea_small, COLOR_RED, 0);
    } else {
        sprintf(buf, "%d\xC2\xB0", g_state.temp_platea);
        lv_label_set_text(lbl_temp_platea_small, buf);
        lv_obj_set_style_text_color(lbl_temp_platea_small, COLOR_WHITE, 0);
    }

    // Update temperature bar
    ui_widget_update_temp_bar(bar_temp, g_state.temp_cielo, g_state.target_cielo);

    // Update power percentages (mostra 100% durante boost)
    int effective_pwr_cielo = oven_state_get_effective_power_cielo();
    int effective_pwr_platea = oven_state_get_effective_power_platea();
    sprintf(buf, "%d %%", effective_pwr_cielo);
    lv_label_set_text(lbl_pwr_cielo, buf);

    sprintf(buf, "%d %%", effective_pwr_platea);
    lv_label_set_text(lbl_pwr_platea, buf);

    // Colore potenze: Arancio in PREHEAT, Rosso in BOOST, Bianco normale
    lv_color_t pwr_color_cielo = COLOR_WHITE;
    lv_color_t pwr_color_platea = COLOR_WHITE;

    if (g_state.mode == OvenMode::PREHEAT) {
        pwr_color_cielo = COLOR_ORANGE;
        pwr_color_platea = COLOR_ORANGE;
    } else if (g_state.mode == OvenMode::BOOST || g_state.boost_cielo) {
        pwr_color_cielo = COLOR_RED;
    }
    if (g_state.mode == OvenMode::BOOST || g_state.boost_platea) {
        pwr_color_platea = COLOR_RED;
    }

    lv_obj_set_style_text_color(lbl_pwr_cielo, pwr_color_cielo, 0);
    lv_obj_set_style_text_color(lbl_pwr_platea, pwr_color_platea, 0);

    // Update resistance icons - lampeggio riflette stato reale SSR (burst firing)
    ui_widget_update_resistance_icon(icon_res_cielo, oven_state_resistance_cielo_on(), ssr_control_cielo_on());
    ui_widget_update_resistance_icon(icon_res_platea, oven_state_resistance_platea_on(), ssr_control_platea_on());

    // Update timer
    if (g_state.timer_minutes != s_prev_timer_min || g_state.timer_seconds != s_prev_timer_sec) {
        s_prev_timer_min = g_state.timer_minutes;
        s_prev_timer_sec = g_state.timer_seconds;
        sprintf(buf, "%d:%02d", g_state.timer_minutes, g_state.timer_seconds);
        lv_label_set_text(lbl_timer, buf);
    }

    // Update play button based on timer state
    if (g_state.timer_active) {
        ui_widget_update_ctrl_btn(btn_play, g_state.timer_paused ? CtrlBtnType::PLAY : CtrlBtnType::PAUSE);
    } else {
        ui_widget_update_ctrl_btn(btn_play, CtrlBtnType::PLAY);
    }

    // Row 1 buttons enabled/disabled based on power state
    ui_widget_set_func_btn_enabled(btn_preheat, FuncBtnType::PREHEAT, g_state.power_on);
    ui_widget_set_func_btn_enabled(btn_maintain, FuncBtnType::MAINTAIN, g_state.power_on);
    ui_widget_set_func_btn_enabled(btn_boost_cielo, FuncBtnType::BOOST_CIELO, g_state.power_on);
    ui_widget_set_func_btn_enabled(btn_boost_platea, FuncBtnType::BOOST_PLATEA, g_state.power_on);

    // Update function buttons active states (only if power is on for row 1)
    if (g_state.power_on) {
        ui_widget_update_func_btn(btn_boost_cielo, FuncBtnType::BOOST_CIELO, g_state.boost_cielo);
        ui_widget_update_func_btn(btn_preheat, FuncBtnType::PREHEAT, g_state.mode == OvenMode::PREHEAT);
        ui_widget_update_func_btn(btn_boost_platea, FuncBtnType::BOOST_PLATEA, g_state.boost_platea);
        ui_widget_update_func_btn(btn_maintain, FuncBtnType::MAINTAIN,
            g_state.mode == OvenMode::MAINTAIN || g_state.mode == OvenMode::READY);
    }
    ui_widget_update_func_btn(btn_power, FuncBtnType::POWER, g_state.power_on);
    ui_widget_update_func_btn(btn_service, FuncBtnType::SERVICE, g_state.service_mode);

    // SERV si attiva solo a forno in temperatura: altrimenti sbiadito
    // (il riferimento al pulsante forza il primo aggiornamento dopo un create)
    static lv_obj_t* s_svc_btn = nullptr;
    static bool s_svc_usable = true;
    bool usable = g_state.service_mode || oven_state_service_available();
    if (btn_service != s_svc_btn || usable != s_svc_usable) {
        s_svc_btn = btn_service;
        s_svc_usable = usable;
        lv_obj_set_style_opa(btn_service, usable ? LV_OPA_COVER : LV_OPA_40, 0);
    }
}

// ============================================
// Destroy
// ============================================
void ui_main_destroy() {
    // Il timer del feedback SALVA scriverebbe su btn_save gia' liberato
    if (save_feedback_timer) {
        lv_timer_del(save_feedback_timer);
        save_feedback_timer = nullptr;
    }

    // Non cancellare mai lo schermo attivo: LVGL va in crash
    if (screen_main && lv_scr_act() != screen_main) {
        lv_obj_del(screen_main);
        screen_main = nullptr;
    }
}

// ============================================
// Get screen
// ============================================
lv_obj_t* ui_main_get_screen() {
    return screen_main;
}

// ============================================
// Event Callbacks
// ============================================
static void on_phase_click(lv_event_t* e) {
    int phase = (int)(intptr_t)lv_event_get_user_data(e);
    if (phase < g_state.num_phases) {
        // Switch alla fase selezionata
        buzzer_beep();
        oven_state_load_phase(phase);
    } else if (phase == g_state.num_phases && g_state.num_phases < MAX_PHASES) {
        // Aggiungi nuova fase copiando i valori dalla fase attiva
        g_state.phases[phase] = g_state.phases[g_state.active_phase];
        g_state.num_phases++;
        oven_state_load_phase(phase);
        buzzer_confirm();
        Serial.printf("Phase: aggiunta fase %d (copiata da fase %d)\n", phase, g_state.active_phase);
    }
}

static void on_phase_long_press(lv_event_t* e) {
    int phase = (int)(intptr_t)lv_event_get_user_data(e);

    // Ignora se fase non usata o unica fase rimasta
    if (phase >= g_state.num_phases) return;
    if (g_state.num_phases <= 1) return;

    if (phase == g_state.num_phases - 1) {
        // Rimuove l'ultima fase
        g_state.num_phases--;
        if (g_state.active_phase >= g_state.num_phases) {
            oven_state_load_phase(g_state.num_phases - 1);
        }
    } else {
        // Rimuove fase intermedia: shift fasi successive
        for (int i = phase; i < g_state.num_phases - 1; i++) {
            g_state.phases[i] = g_state.phases[i + 1];
        }
        g_state.num_phases--;
        if (g_state.active_phase >= g_state.num_phases) {
            oven_state_load_phase(g_state.num_phases - 1);
        } else if (g_state.active_phase == phase) {
            oven_state_load_phase(phase);  // Ricarica (ora ha i valori della vecchia fase+1)
        }
    }
    buzzer_confirm();
    Serial.printf("Phase: rimossa fase %d, num_phases=%d\n", phase, g_state.num_phases);
}

static void on_set_cielo_click(lv_event_t* e) {
    buzzer_beep();
    ui_keypad_show(&g_state.target_cielo, "SET CIELO", 0, 500);
    oven_state_register_activity();
}

static void on_set_platea_click(lv_event_t* e) {
    buzzer_beep();
    ui_keypad_show(&g_state.target_platea, "SET PLATEA", 0, 500);
    oven_state_register_activity();
}

static void on_pwr_cielo_click(lv_event_t* e) {
    buzzer_beep();
    // Non editabile durante PREHEAT, MAINTAIN, READY (potenza gestita automaticamente)
    if (g_state.mode == OvenMode::PREHEAT ||
        g_state.mode == OvenMode::MAINTAIN ||
        g_state.mode == OvenMode::READY) {
        return;
    }
    ui_keypad_show(&g_state.power_cielo, "PWR CIELO %", 5, 100);
    oven_state_register_activity();
}

static void on_pwr_platea_click(lv_event_t* e) {
    buzzer_beep();
    if (g_state.mode == OvenMode::PREHEAT ||
        g_state.mode == OvenMode::MAINTAIN ||
        g_state.mode == OvenMode::READY) {
        return;
    }
    ui_keypad_show(&g_state.power_platea, "PWR PLATEA %", 5, 100);
    oven_state_register_activity();
}

static void on_restart_click(lv_event_t* e) {
    buzzer_beep();
    g_state.timer_minutes = 0;
    g_state.timer_seconds = 0;
    g_state.timer_active = false;
    g_state.timer_paused = false;
    g_state.timer_expired = false;
    oven_state_register_activity();
    Serial.println("Timer reset");
}

static void on_play_click(lv_event_t* e) {
    buzzer_beep();
    if (!g_state.power_on) return;

    if (g_state.timer_active) {
        // Toggle pause (la pausa ferma solo timer, NON il riscaldamento)
        g_state.timer_paused = !g_state.timer_paused;
        Serial.printf("Timer: %s\n", g_state.timer_paused ? "paused" : "resumed");
    } else {
        // Da MAINTAIN o READY, avvia cottura
        // oven_state_start_cooking() carica il timer dalla fase se necessario
        oven_state_start_cooking();
    }
    oven_state_register_activity();
}

static void on_timer_click(lv_event_t* e) {
    buzzer_beep();
    ui_keypad_show_time(&g_state.timer_minutes, &g_state.timer_seconds, "TIMER (m:ss)");
    oven_state_register_activity();
}

static void on_save_feedback_timer(lv_timer_t* timer) {
    (void)timer;
    lv_obj_set_style_border_color(btn_save, COLOR_GRAY, 0);
    lv_obj_set_style_border_width(btn_save, 1, 0);
    save_feedback_timer = nullptr;
}

static void on_save_click(lv_event_t* e) {
    buzzer_confirm();
    programs_save_current_phase_params();

    // Feedback visivo: bordo verde per 1 secondo
    lv_obj_set_style_border_color(btn_save, COLOR_GREEN, 0);
    lv_obj_set_style_border_width(btn_save, 3, 0);

    // Cancella timer precedente se spam-click
    if (save_feedback_timer) {
        lv_timer_del(save_feedback_timer);
    }
    save_feedback_timer = lv_timer_create(on_save_feedback_timer, 1000, NULL);
    lv_timer_set_repeat_count(save_feedback_timer, 1);

    oven_state_register_activity();
}

static void on_boost_cielo_click(lv_event_t* e) {
    buzzer_beep();
    if (!g_state.power_on) return;
    oven_state_toggle_boost_cielo();
    Serial.printf("Boost Cielo: %s\n", g_state.boost_cielo ? "ON" : "OFF");
}

static void on_preheat_click(lv_event_t* e) {
    buzzer_beep();
    if (!g_state.power_on) return;

    if (g_state.mode == OvenMode::PREHEAT) {
        // Disable preheat, go to maintain
        oven_state_start_maintain();
    } else {
        oven_state_start_preheat();
    }
    Serial.printf("Mode: %s\n", oven_state_get_mode_string());
}

static void on_boost_platea_click(lv_event_t* e) {
    buzzer_beep();
    if (!g_state.power_on) return;
    oven_state_toggle_boost_platea();
    Serial.printf("Boost Platea: %s\n", g_state.boost_platea ? "ON" : "OFF");
}

static void on_maintain_click(lv_event_t* e) {
    buzzer_beep();
    if (!g_state.power_on) return;

    if (g_state.mode == OvenMode::MAINTAIN) {
        // Go to ready
        g_state.mode = OvenMode::READY;
    } else {
        oven_state_start_maintain();
    }
    Serial.printf("Mode: %s\n", oven_state_get_mode_string());
}

static void on_power_click(lv_event_t* e) {
    buzzer_beep();
    g_state.power_on = !g_state.power_on;
    if (!g_state.power_on) {
        oven_state_go_standby();
    } else if (g_state.error_fan) {
        // Reset fan error on power ON
        buzzer_stop();
        g_state.error_fan = false;
        g_state.error_msg_fan[0] = '\0';
        g_state.error_count_fan = 0;
    }
    oven_state_register_activity();
    Serial.printf("Power: %s\n", g_state.power_on ? "ON" : "OFF");
}

static bool power_is_armed() {
    return g_state.power_on;
}

static void power_tap() {
    on_power_click(nullptr);  // accensione
}

static void power_hold() {
    s_power_hint = false;
    on_power_click(nullptr);  // spegnimento
    buzzer_confirm();
}

static void power_short() {
    buzzer_beep();
    s_power_hint = true;
    s_power_hint_start = millis();
}

static void on_prog_click(lv_event_t* e) {
    buzzer_beep();
    ui_programs_show();
    oven_state_register_activity();
}

static void on_service_click(lv_event_t* e) {
    if (oven_state_set_service_mode(!g_state.service_mode)) {
        buzzer_beep();
    } else {
        buzzer_error();  // forno spento o ancora in preriscaldamento
    }
    oven_state_register_activity();
}

static void on_menu_click(lv_event_t* e) {
    // Pressione lunga: cambio modalita' gia' richiesto, non aprire il menu
    if (ui_mode_consume_long_press()) return;

    buzzer_beep();
    ui_settings_open();
    oven_state_register_activity();
}

// ============================================
// Public event callbacks (for header compatibility)
// ============================================
void ui_main_on_phase_click(lv_event_t* e) {
    on_phase_click(e);
}

void ui_main_on_set_click(lv_event_t* e) {
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    if (which == 0) {
        on_set_cielo_click(e);
    } else {
        on_set_platea_click(e);
    }
}

void ui_main_on_pwr_click(lv_event_t* e) {
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    if (which == 0) {
        on_pwr_cielo_click(e);
    } else {
        on_pwr_platea_click(e);
    }
}

void ui_main_on_timer_click(lv_event_t* e) {
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    switch (action) {
        case 0: on_restart_click(e); break;
        case 1: on_play_click(e); break;
        case 2: on_timer_click(e); break;
    }
}

void ui_main_on_boost_click(lv_event_t* e) {
    int which = (int)(intptr_t)lv_event_get_user_data(e);
    if (which == 0) {
        on_boost_cielo_click(e);
    } else {
        on_boost_platea_click(e);
    }
}
