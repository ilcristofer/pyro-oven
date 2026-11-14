/**
 * Schermata di aggiornamento firmware in corso
 */

#include "ui_ota_progress.h"
#include "ui_ota.h"
#include "ui_theme.h"
#include "hardware_pins.h"
#include "buzzer.h"
#include <lvgl.h>
#include <Arduino.h>

#define REDRAW_MS      250    // ogni ridisegno ferma l'upload per qualche decina di ms
#define ERROR_HIDE_MS  6000

static lv_obj_t* s_panel = nullptr;
static lv_obj_t* s_lbl_pct = nullptr;
static lv_obj_t* s_bar = nullptr;
static lv_obj_t* s_lbl_size = nullptr;
static lv_obj_t* s_lbl_speed = nullptr;
static lv_obj_t* s_lbl_info = nullptr;
static lv_obj_t* s_lbl_state = nullptr;

static unsigned long s_start_ms = 0;
static unsigned long s_last_redraw = 0;
static unsigned long s_error_at = 0;
static int s_prev_pct = -1;

static lv_obj_t* make_label(const lv_font_t* font, lv_color_t color, int y) {
    lv_obj_t* l = lv_label_create(s_panel);
    lv_obj_set_width(l, SCREEN_WIDTH - 20);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, color, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(l, LV_ALIGN_TOP_MID, 0, y);
    return l;
}

static void create_panel() {
    // Pannello a tutto schermo sul layer top: copre qualsiasi schermata e,
    // essendo cliccabile, non lascia arrivare i tocchi ai pulsanti sotto
    s_panel = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_panel);
    lv_obj_set_size(s_panel, SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_obj_set_style_bg_color(s_panel, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(s_panel, LV_OPA_COVER, 0);
    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_panel, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t* title = make_label(&lv_font_montserrat_20, COLOR_ORANGE, 30);
    lv_label_set_text(title, LV_SYMBOL_DOWNLOAD "  AGGIORNAMENTO\nFIRMWARE");

    lv_obj_t* warn = make_label(&lv_font_montserrat_16, COLOR_YELLOW, 90);
    lv_label_set_text(warn, LV_SYMBOL_WARNING " Non togliere corrente");

    s_lbl_pct = make_label(&lv_font_montserrat_48, COLOR_WHITE, 140);

    s_bar = lv_bar_create(s_panel);
    lv_obj_set_size(s_bar, SCREEN_WIDTH - 40, 26);
    lv_obj_align(s_bar, LV_ALIGN_TOP_MID, 0, 205);
    lv_bar_set_range(s_bar, 0, 100);
    lv_obj_set_style_bg_color(s_bar, COLOR_DARK_GRAY, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_bar, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_bar, 6, 0);
    lv_obj_set_style_radius(s_bar, 6, LV_PART_INDICATOR);

    s_lbl_size = make_label(&lv_font_montserrat_18, COLOR_WHITE, 245);
    s_lbl_speed = make_label(&lv_font_montserrat_16, COLOR_WHITE, 275);
    s_lbl_info = make_label(&lv_font_montserrat_14, COLOR_WHITE, 325);
    s_lbl_state = make_label(&lv_font_montserrat_20, COLOR_ORANGE, 420);
}

static void redraw() {
    // loop() e' fermo nell'upload: lv_timer_handler() non gira. Si ridisegna
    // direttamente, senza leggere il touch ne' far girare i timer LVGL
    lv_refr_now(NULL);
    s_last_redraw = millis();
}

void ui_ota_progress_begin(const char* client, bool was_on) {
    if (!s_panel) create_panel();

    s_start_ms = millis();
    s_error_at = 0;
    s_prev_pct = -1;

    lv_label_set_text(s_lbl_pct, "0%");
    lv_bar_set_value(s_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_bar, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_label_set_text(s_lbl_size, "");
    lv_label_set_text(s_lbl_speed, "");
    lv_label_set_text_fmt(s_lbl_info,
                          "Firmware attuale: v" FW_VERSION "\n"
                          "Inviato da: %s\n"
                          "%s",
                          client,
                          was_on ? "Forno messo in standby, resistenze spente"
                                 : "Resistenze spente");
    lv_label_set_text(s_lbl_state, "Ricezione in corso...");
    lv_obj_set_style_text_color(s_lbl_state, COLOR_ORANGE, 0);

    lv_obj_clear_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_panel);
    redraw();
}

void ui_ota_progress_update(size_t received, size_t total) {
    if (!s_panel) return;

    int pct = total > 0 ? (int)min((size_t)100, received * 100 / total) : 0;
    if (pct == s_prev_pct && millis() - s_last_redraw < 1000) return;
    if (millis() - s_last_redraw < REDRAW_MS) return;
    s_prev_pct = pct;

    lv_label_set_text_fmt(s_lbl_pct, "%d%%", pct);
    lv_bar_set_value(s_bar, pct, LV_ANIM_OFF);

    if (total > 0) {
        lv_label_set_text_fmt(s_lbl_size, "%u / %u KB",
                              (unsigned)(received / 1024), (unsigned)(total / 1024));
    } else {
        lv_label_set_text_fmt(s_lbl_size, "%u KB", (unsigned)(received / 1024));
    }

    unsigned long elapsed = millis() - s_start_ms;
    if (elapsed >= 1000) {
        unsigned kbs = (unsigned)(received / elapsed);   // byte/ms = KB/s circa
        if (kbs > 0 && total > received) {
            unsigned left_s = (unsigned)((total - received) / 1024 / kbs) + 1;
            lv_label_set_text_fmt(s_lbl_speed, "%u KB/s  -  mancano ~%u s", kbs, left_s);
        } else {
            lv_label_set_text_fmt(s_lbl_speed, "%u KB/s", kbs);
        }
    }

    redraw();
}

void ui_ota_progress_end(bool ok, const char* msg) {
    if (!s_panel) return;

    unsigned long secs = (millis() - s_start_ms + 500) / 1000;
    if (ok) {
        lv_label_set_text(s_lbl_pct, "100%");
        lv_bar_set_value(s_bar, 100, LV_ANIM_OFF);
        lv_obj_set_style_bg_color(s_bar, COLOR_GREEN, LV_PART_INDICATOR);
        lv_label_set_text_fmt(s_lbl_speed, "Completato in %lu s", secs);
        lv_label_set_text(s_lbl_state, LV_SYMBOL_OK " Riavvio in corso...");
        lv_obj_set_style_text_color(s_lbl_state, COLOR_GREEN, 0);
        // La schermata Aggiorna Firmware ha gia' il suo suono di conferma
        if (!ui_ota_is_visible()) buzzer_confirm();
    } else {
        lv_obj_set_style_bg_color(s_bar, COLOR_RED, LV_PART_INDICATOR);
        lv_label_set_text_fmt(s_lbl_speed, "%s", msg ? msg : "Errore");
        lv_label_set_text(s_lbl_state, LV_SYMBOL_CLOSE " Aggiornamento fallito\n"
                                       "resta il firmware attuale");
        lv_obj_set_style_text_color(s_lbl_state, COLOR_RED, 0);
        lv_obj_align(s_lbl_state, LV_ALIGN_TOP_MID, 0, 405);
        s_error_at = millis();
        buzzer_error();
    }
    redraw();
}

void ui_ota_progress_tick() {
    if (s_panel && s_error_at > 0 && millis() - s_error_at >= ERROR_HIDE_MS) {
        s_error_at = 0;
        lv_obj_align(s_lbl_state, LV_ALIGN_TOP_MID, 0, 420);
        lv_obj_add_flag(s_panel, LV_OBJ_FLAG_HIDDEN);
    }
}
