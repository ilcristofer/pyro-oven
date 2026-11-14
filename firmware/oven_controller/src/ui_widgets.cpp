/**
 * UI Custom Widgets Implementation - Effeuno style
 */

#include "ui_widgets.h"
#include "ui_theme.h"

// ============================================
// Phase Gauge
// ============================================
lv_obj_t* ui_widget_create_phase_gauge(lv_obj_t* parent, int phase_index) {
    // Create container for gauge
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, GAUGE_SIZE, GAUGE_SIZE);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Create arc for the gauge background (tick marks simulation)
    lv_obj_t* arc = lv_arc_create(cont);
    lv_obj_set_size(arc, GAUGE_SIZE - 4, GAUGE_SIZE - 4);
    lv_obj_center(arc);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_value(arc, 0);
    lv_obj_set_style_arc_color(arc, COLOR_DARK_GRAY, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, COLOR_ORANGE, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    // Store phase index
    lv_obj_set_user_data(cont, (void*)(intptr_t)phase_index);

    return cont;
}

void ui_widget_update_phase_gauge(lv_obj_t* gauge, int phase_index, bool is_active, int progress) {
    // Get the arc child
    lv_obj_t* arc = lv_obj_get_child(gauge, 0);
    if (arc) {
        lv_arc_set_value(arc, progress);

        // Change color based on active state
        if (is_active) {
            lv_obj_set_style_arc_color(arc, COLOR_GREEN, LV_PART_MAIN);
        } else {
            lv_obj_set_style_arc_color(arc, COLOR_DARK_GRAY, LV_PART_MAIN);
        }
    }
}

// ============================================
// Temperature Bar (alta tutta la sezione - 4° colonna)
// ============================================
lv_obj_t* ui_widget_create_temp_bar(lv_obj_t* parent) {
    // Container for the segmented bar
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 22, 133);  // Ridotta di 1 tacca
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Create 36 segments (triplo, altezza 1/3)
    int num_segments = 36;
    int seg_height = 3;
    int seg_spacing = 1;
    int total_height = 133;

    // Colori: 9 rossi, 12 gialli, 15 verdi (triplicato)
    for (int i = 0; i < num_segments; i++) {
        lv_obj_t* seg = lv_obj_create(cont);
        lv_obj_set_size(seg, 18, seg_height);
        lv_obj_set_pos(seg, 2, total_height - (i + 1) * (seg_height + seg_spacing));

        // Determina colore in base alla posizione
        lv_color_t color;
        if (i < 9) {
            color = COLOR_RED;       // Bottom 9 - red
        } else if (i < 21) {
            color = COLOR_YELLOW;    // Middle 12 - yellow
        } else {
            color = COLOR_GREEN;     // Top 15 - green
        }

        lv_obj_set_style_bg_color(seg, color, 0);
        lv_obj_set_style_bg_opa(seg, LV_OPA_30, 0);  // Dim by default
        lv_obj_set_style_radius(seg, 0, 0);
        lv_obj_set_style_border_width(seg, 0, 0);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
    }

    return cont;
}

void ui_widget_update_temp_bar(lv_obj_t* bar, int current_temp, int target_temp) {
    // Calculate how many segments to light up (0-36)
    int num_segments = 36;
    int ratio = 0;
    if (target_temp > 0) {
        ratio = (current_temp * num_segments) / target_temp;
        if (ratio > num_segments) ratio = num_segments;
        if (ratio < 0) ratio = 0;
    }

    // Update segment opacity
    for (int i = 0; i < num_segments; i++) {
        lv_obj_t* seg = lv_obj_get_child(bar, i);
        if (seg) {
            if (i < ratio) {
                lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);  // Fully lit
            } else {
                lv_obj_set_style_bg_opa(seg, LV_OPA_30, 0);     // Dim
            }
        }
    }
}

// ============================================
// Resistance Icon (drawn with lines) - con caps orizzontali
// ============================================
lv_obj_t* ui_widget_create_resistance_icon(lv_obj_t* parent) {
    // Container
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 40, 20);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Draw zigzag resistance con caps orizzontali (9 punti)
    // Caps 5px, zigzag 30px nel mezzo, stessa larghezza totale 40px
    static lv_point_t res_points[9];
    res_points[0] = {0, 10};       // left cap start
    res_points[1] = {5, 10};       // left cap end
    res_points[2] = {9, 2};        // zigzag
    res_points[3] = {15, 18};
    res_points[4] = {21, 2};
    res_points[5] = {27, 18};
    res_points[6] = {31, 2};
    res_points[7] = {35, 10};      // right cap start
    res_points[8] = {40, 10};      // right cap end

    lv_obj_t* line = lv_line_create(cont);
    lv_line_set_points(line, res_points, 9);
    lv_obj_set_style_line_color(line, COLOR_WHITE, 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);

    return cont;
}

void ui_widget_update_resistance_icon(lv_obj_t* cont, bool is_on, bool blink_state) {
    lv_obj_t* line = lv_obj_get_child(cont, 0);
    if (line) {
        if (is_on) {
            // Blink red when heating
            lv_color_t color = blink_state ? COLOR_RED : COLOR_DARK_GRAY;
            lv_obj_set_style_line_color(line, color, 0);
        } else {
            // White when off
            lv_obj_set_style_line_color(line, COLOR_WHITE, 0);
        }
    }
}

// ============================================
// SET Icons (rettangolo bianco con linea nera)
// ============================================
lv_obj_t* ui_widget_create_set_icon_cielo(lv_obj_t* parent) {
    // Container
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 32, 20);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Rettangolo bianco (sfondo) - piu largo
    lv_obj_t* rect = lv_obj_create(cont);
    lv_obj_set_size(rect, 28, 16);
    lv_obj_set_pos(rect, 2, 2);
    lv_obj_set_style_bg_color(rect, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0);
    lv_obj_set_style_border_width(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_clear_flag(rect, LV_OBJ_FLAG_SCROLLABLE);

    // Linea nera in ALTO (cielo)
    lv_obj_t* line = lv_obj_create(rect);
    lv_obj_set_size(line, 22, 3);
    lv_obj_set_pos(line, 3, 2);  // In alto
    lv_obj_set_style_bg_color(line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_border_width(line, 0, 0);

    return cont;
}

lv_obj_t* ui_widget_create_set_icon_platea(lv_obj_t* parent) {
    // Container
    lv_obj_t* cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 32, 20);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Rettangolo bianco (sfondo) - piu largo
    lv_obj_t* rect = lv_obj_create(cont);
    lv_obj_set_size(rect, 28, 16);
    lv_obj_set_pos(rect, 2, 2);
    lv_obj_set_style_bg_color(rect, COLOR_WHITE, 0);
    lv_obj_set_style_bg_opa(rect, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(rect, 0, 0);
    lv_obj_set_style_border_width(rect, 0, 0);
    lv_obj_set_style_pad_all(rect, 0, 0);
    lv_obj_clear_flag(rect, LV_OBJ_FLAG_SCROLLABLE);

    // Linea nera in BASSO (platea)
    lv_obj_t* line = lv_obj_create(rect);
    lv_obj_set_size(line, 22, 3);
    lv_obj_set_pos(line, 3, 11);  // In basso
    lv_obj_set_style_bg_color(line, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_border_width(line, 0, 0);

    return cont;
}

// ============================================
// Status Bar
// ============================================
lv_obj_t* ui_widget_create_status_bar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, SCREEN_WIDTH, STATUS_HEIGHT);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, COLOR_BG, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    return bar;
}

void ui_widget_update_status_bar(lv_obj_t* bar, OvenMode mode, bool timer_expired, bool blink) {
    lv_color_t bg_color = COLOR_BG;

    if (timer_expired && blink) {
        bg_color = COLOR_RED;
    } else {
        switch (mode) {
            case OvenMode::STANDBY:
                bg_color = COLOR_GRAY;
                break;
            case OvenMode::READY:
                bg_color = COLOR_GREEN;
                break;
            case OvenMode::PREHEAT:
                bg_color = COLOR_ORANGE;
                break;
            case OvenMode::MAINTAIN:
                bg_color = COLOR_YELLOW;
                break;
            case OvenMode::COOKING:
                bg_color = COLOR_BORDEAUX;
                break;
            case OvenMode::BOOST:
                bg_color = COLOR_RED;
                break;
            default:
                bg_color = COLOR_BG;
                break;
        }
    }

    lv_obj_set_style_bg_color(bar, bg_color, 0);
}

// ============================================
// Function Button Icons (drawn with LVGL primitives)
// ============================================

// Storage for line points (must be static to persist)
static lv_point_t zig1_pts[5];
static lv_point_t zig2_pts[5];
static lv_point_t zig3_pts[5];
// Storage for 9-point zigzag (7 zigzag + 2 horizontal caps)
static lv_point_t zig9_pts1[9];
static lv_point_t zig9_pts2[9];
static lv_point_t zig9_pts3[9];  // MAINTAIN
static lv_point_t zig9_pts4[9];  // BOOST CIELO
static lv_point_t zig9_pts5[9];  // BOOST PLATEA

// Draw small resistance zigzag at position (5 points)
static lv_obj_t* draw_small_zigzag(lv_obj_t* parent, lv_point_t* pts, int x, int y, int w, int h) {
    int sw = w / 4;  // segment width
    pts[0] = {(lv_coord_t)(x), (lv_coord_t)(y + h/2)};
    pts[1] = {(lv_coord_t)(x + sw), (lv_coord_t)(y)};
    pts[2] = {(lv_coord_t)(x + sw*2), (lv_coord_t)(y + h)};
    pts[3] = {(lv_coord_t)(x + sw*3), (lv_coord_t)(y)};
    pts[4] = {(lv_coord_t)(x + w), (lv_coord_t)(y + h/2)};

    lv_obj_t* line = lv_line_create(parent);
    lv_line_set_points(line, pts, 5);
    lv_obj_set_style_line_color(line, COLOR_WHITE, 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

// Draw resistance zigzag with 9 points (horizontal caps + zigzag)
static lv_obj_t* draw_resistance_zigzag(lv_obj_t* parent, lv_point_t* pts, int x, int y, int w, int h) {
    // 9 points: left cap (6px), zigzag, right cap (6px)
    int cap = 6;  // horizontal cap length
    pts[0] = {(lv_coord_t)(x), (lv_coord_t)(y + h/2)};           // left cap start
    pts[1] = {(lv_coord_t)(x + cap), (lv_coord_t)(y + h/2)};     // left cap end / zigzag start
    pts[2] = {(lv_coord_t)(x + cap + w*6/40), (lv_coord_t)(y)};
    pts[3] = {(lv_coord_t)(x + cap + w*12/40), (lv_coord_t)(y + h)};
    pts[4] = {(lv_coord_t)(x + cap + w*20/40), (lv_coord_t)(y)};
    pts[5] = {(lv_coord_t)(x + cap + w*28/40), (lv_coord_t)(y + h)};
    pts[6] = {(lv_coord_t)(x + cap + w*34/40), (lv_coord_t)(y)};
    pts[7] = {(lv_coord_t)(x + cap + w), (lv_coord_t)(y + h/2)};  // zigzag end / right cap start
    pts[8] = {(lv_coord_t)(x + cap + w + cap), (lv_coord_t)(y + h/2)}; // right cap end

    lv_obj_t* line = lv_line_create(parent);
    lv_line_set_points(line, pts, 9);
    lv_obj_set_style_line_color(line, COLOR_WHITE, 0);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

// Draw lightbulb icon
static void draw_icon_light(lv_obj_t* btn) {
    // Bulb body (circle at top)
    lv_obj_t* bulb = lv_obj_create(btn);
    lv_obj_set_size(bulb, 26, 26);
    lv_obj_align(bulb, LV_ALIGN_TOP_MID, 0, 2);
    lv_obj_set_style_radius(bulb, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(bulb, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bulb, 2, 0);
    lv_obj_set_style_border_color(bulb, COLOR_WHITE, 0);
    lv_obj_clear_flag(bulb, LV_OBJ_FLAG_SCROLLABLE);

    // Base (small rectangle at bottom)
    lv_obj_t* base = lv_obj_create(btn);
    lv_obj_set_size(base, 14, 10);
    lv_obj_align(base, LV_ALIGN_BOTTOM_MID, 0, -4);
    lv_obj_set_style_radius(base, 2, 0);
    lv_obj_set_style_bg_opa(base, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(base, 2, 0);
    lv_obj_set_style_border_color(base, COLOR_WHITE, 0);
    lv_obj_clear_flag(base, LV_OBJ_FLAG_SCROLLABLE);
}

// Le icone stanno in un pulsante senza padding (contenuto 70x44): icona in
// alto, scritta piccola in basso. Prima le icone da sole erano criptiche
#define ICON_ZIG_W   22
#define ICON_ZIG_X   ((FUNC_BTN_WIDTH - 2 - ICON_ZIG_W - 12) / 2)

static lv_obj_t* draw_btn_caption(lv_obj_t* btn, const char* text) {
    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, COLOR_WHITE, 0);
    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -3);
    return lbl;
}

// PREHEAT: 2 resistenze (cielo e platea) + "PRERISC."
static void draw_icon_preheat(lv_obj_t* btn) {
    draw_resistance_zigzag(btn, zig9_pts1, ICON_ZIG_X, 3, ICON_ZIG_W, 8);
    draw_resistance_zigzag(btn, zig9_pts2, ICON_ZIG_X, 15, ICON_ZIG_W, 8);
    draw_btn_caption(btn, "PRERISC.");
}

// MAINTAIN: una resistenza + "MANT."
static void draw_icon_maintain(lv_obj_t* btn) {
    draw_resistance_zigzag(btn, zig9_pts3, ICON_ZIG_X, 8, ICON_ZIG_W, 10);
    draw_btn_caption(btn, "MANT.");
}

// BOOST CIELO: resistenza in alto + "BOOST" con freccia su
static void draw_icon_boost_cielo(lv_obj_t* btn) {
    draw_resistance_zigzag(btn, zig9_pts4, ICON_ZIG_X, 3, ICON_ZIG_W, 8);
    draw_btn_caption(btn, "BOOST " LV_SYMBOL_UP);
}

// BOOST PLATEA: resistenza piu' in basso + "BOOST" con freccia giu'
static void draw_icon_boost_platea(lv_obj_t* btn) {
    draw_resistance_zigzag(btn, zig9_pts5, ICON_ZIG_X, 15, ICON_ZIG_W, 8);
    draw_btn_caption(btn, "BOOST " LV_SYMBOL_DOWN);
}

static void draw_func_btn_icon(lv_obj_t* btn, FuncBtnType type) {
    // Clear existing children
    while (lv_obj_get_child_cnt(btn) > 0) {
        lv_obj_del(lv_obj_get_child(btn, 0));
    }

    lv_obj_t* icon = NULL;

    switch (type) {
        case FuncBtnType::PREHEAT:
            draw_icon_preheat(btn);
            break;

        case FuncBtnType::MAINTAIN:
            draw_icon_maintain(btn);
            break;

        case FuncBtnType::BOOST_CIELO:
            draw_icon_boost_cielo(btn);
            break;

        case FuncBtnType::BOOST_PLATEA:
            draw_icon_boost_platea(btn);
            break;

        case FuncBtnType::POWER:
            icon = lv_label_create(btn);
            lv_label_set_text(icon, LV_SYMBOL_POWER);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
            lv_obj_center(icon);
            break;

        case FuncBtnType::SERVICE: {
            // Ciclo = infornate in sequenza, con etichetta sotto
            icon = lv_label_create(btn);
            lv_label_set_text(icon, LV_SYMBOL_LOOP);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_18, 0);
            lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
            lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 3);
            draw_btn_caption(btn, "SERVIZIO");
            break;
        }

        case FuncBtnType::LIGHT:
            draw_icon_light(btn);
            break;

        case FuncBtnType::PROGRAMS:
            icon = lv_label_create(btn);
            lv_label_set_text(icon, "P");
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_28, 0);
            lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
            lv_obj_center(icon);
            break;

        case FuncBtnType::MENU:
            icon = lv_label_create(btn);
            lv_label_set_text(icon, LV_SYMBOL_LIST);
            lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);
            lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
            lv_obj_center(icon);
            break;
    }
}

lv_obj_t* ui_widget_create_func_btn(lv_obj_t* parent, FuncBtnType type) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, FUNC_BTN_WIDTH, FUNC_BTN_HEIGHT);
    lv_obj_add_style(btn, &style_button, 0);
    lv_obj_add_style(btn, &style_button_pressed, LV_STATE_PRESSED);
    // Niente padding del tema: icona e scritta hanno bisogno di tutta
    // l'altezza (con il padding la scritta SERV finiva sopra l'icona)
    lv_obj_set_style_pad_all(btn, 0, 0);

    // Draw the icon
    draw_func_btn_icon(btn, type);

    // Store type in user data
    lv_obj_set_user_data(btn, (void*)(intptr_t)type);

    return btn;
}

void ui_widget_update_func_btn(lv_obj_t* btn, FuncBtnType type, bool active) {
    if (active) {
        // Active: colored border and/or background
        switch (type) {
            case FuncBtnType::LIGHT: {
                lv_obj_set_style_border_color(btn, COLOR_YELLOW, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                // Icona gialla
                for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
                    lv_obj_t* child = lv_obj_get_child(btn, i);
                    lv_obj_set_style_border_color(child, COLOR_YELLOW, 0);
                }
                break;
            }
            case FuncBtnType::BOOST_CIELO:
            case FuncBtnType::BOOST_PLATEA: {
                lv_obj_set_style_border_color(btn, COLOR_RED, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                lv_obj_set_style_bg_color(btn, COLOR_RED, 0);
                // Icone nere su sfondo rosso
                for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
                    lv_obj_t* child = lv_obj_get_child(btn, i);
                    lv_obj_set_style_line_color(child, lv_color_black(), 0);
                    lv_obj_set_style_text_color(child, lv_color_black(), 0);
                }
                break;
            }
            case FuncBtnType::PREHEAT: {
                lv_obj_set_style_border_color(btn, COLOR_ORANGE, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                lv_obj_set_style_bg_color(btn, COLOR_ORANGE, 0);
                // Icone nere su sfondo arancione
                for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
                    lv_obj_t* child = lv_obj_get_child(btn, i);
                    lv_obj_set_style_line_color(child, lv_color_black(), 0);
                    lv_obj_set_style_text_color(child, lv_color_black(), 0);
                }
                break;
            }
            case FuncBtnType::MAINTAIN: {
                lv_obj_set_style_border_color(btn, COLOR_YELLOW, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                lv_obj_set_style_bg_color(btn, COLOR_YELLOW, 0);
                // Icone nere su sfondo giallo
                for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
                    lv_obj_t* child = lv_obj_get_child(btn, i);
                    lv_obj_set_style_line_color(child, lv_color_black(), 0);
                    lv_obj_set_style_text_color(child, lv_color_black(), 0);
                }
                break;
            }
            case FuncBtnType::SERVICE: {
                lv_obj_set_style_border_color(btn, COLOR_GREEN_READY, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                lv_obj_set_style_bg_color(btn, COLOR_GREEN_READY, 0);
                for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
                    lv_obj_t* child = lv_obj_get_child(btn, i);
                    lv_obj_set_style_text_color(child, COLOR_WHITE, 0);
                }
                break;
            }
            case FuncBtnType::POWER: {
                lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                lv_obj_set_style_bg_color(btn, COLOR_GREEN, 0);
                // Icona nera su sfondo verde
                lv_obj_t* icon = lv_obj_get_child(btn, 0);
                if (icon) lv_obj_set_style_text_color(icon, lv_color_black(), 0);
                break;
            }
            default:
                lv_obj_set_style_border_color(btn, COLOR_WHITE, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                break;
        }
    } else {
        // Inactive: gray border, restore colors
        lv_obj_set_style_border_color(btn, COLOR_GRAY, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_bg_color(btn, COLOR_BUTTON_BG, 0);
        // Ripristina colori icone
        for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
            lv_obj_t* child = lv_obj_get_child(btn, i);
            lv_obj_set_style_line_color(child, COLOR_WHITE, 0);
            lv_obj_set_style_text_color(child, COLOR_WHITE, 0);
            lv_obj_set_style_border_color(child, COLOR_WHITE, 0);
        }
    }
}

void ui_widget_set_func_btn_enabled(lv_obj_t* btn, FuncBtnType type, bool enabled) {
    if (enabled) {
        // Enabled: restore clickable and normal style
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_border_color(btn, COLOR_GRAY, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        // Ripristina colori icone
        for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
            lv_obj_t* child = lv_obj_get_child(btn, i);
            lv_obj_set_style_line_color(child, COLOR_WHITE, 0);
            lv_obj_set_style_text_color(child, COLOR_WHITE, 0);
            lv_obj_set_style_border_color(child, COLOR_WHITE, 0);
        }
    } else {
        // Disabled: light gray appearance, not clickable
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_color(btn, COLOR_DARK_GRAY, 0);
        lv_obj_set_style_border_color(btn, COLOR_DARK_GRAY, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        // Icone grigio chiaro
        for (int i = 0; i < (int)lv_obj_get_child_cnt(btn); i++) {
            lv_obj_t* child = lv_obj_get_child(btn, i);
            lv_obj_set_style_line_color(child, COLOR_GRAY, 0);
            lv_obj_set_style_text_color(child, COLOR_GRAY, 0);
            lv_obj_set_style_border_color(child, COLOR_GRAY, 0);
        }
    }
}

// ============================================
// Control Button
// ============================================
lv_obj_t* ui_widget_create_ctrl_btn(lv_obj_t* parent, CtrlBtnType type) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, CTRL_BTN_SIZE, CTRL_BTN_SIZE);
    lv_obj_add_style(btn, &style_button, 0);
    lv_obj_add_style(btn, &style_button_pressed, LV_STATE_PRESSED);

    lv_obj_t* icon = lv_label_create(btn);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_24, 0);

    switch (type) {
        case CtrlBtnType::RESTART:
            lv_label_set_text(icon, LV_SYMBOL_REFRESH);
            lv_obj_set_style_text_color(icon, COLOR_ORANGE, 0);
            lv_obj_set_style_border_color(btn, COLOR_ORANGE, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
            break;

        case CtrlBtnType::PLAY:
            lv_label_set_text(icon, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(icon, COLOR_GREEN, 0);
            lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
            break;

        case CtrlBtnType::PAUSE:
            lv_label_set_text(icon, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
            lv_obj_set_style_border_color(btn, COLOR_WHITE, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
            break;

        case CtrlBtnType::STOP:
            lv_label_set_text(icon, LV_SYMBOL_STOP);
            lv_obj_set_style_text_color(icon, COLOR_RED, 0);
            lv_obj_set_style_border_color(btn, COLOR_RED, 0);
            lv_obj_set_style_border_width(btn, 2, 0);
            break;

        case CtrlBtnType::SAVE:
            lv_label_set_text(icon, LV_SYMBOL_SAVE);
            lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
            lv_obj_set_style_border_color(btn, COLOR_GRAY, 0);
            lv_obj_set_style_border_width(btn, 1, 0);
            break;
    }

    lv_obj_center(icon);
    lv_obj_set_user_data(btn, (void*)(intptr_t)type);

    return btn;
}

void ui_widget_update_ctrl_btn(lv_obj_t* btn, CtrlBtnType type) {
    lv_obj_t* icon = lv_obj_get_child(btn, 0);
    if (!icon) return;

    switch (type) {
        case CtrlBtnType::PLAY:
            lv_label_set_text(icon, LV_SYMBOL_PLAY);
            lv_obj_set_style_text_color(icon, COLOR_GREEN, 0);
            lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
            break;

        case CtrlBtnType::PAUSE:
            lv_label_set_text(icon, LV_SYMBOL_PAUSE);
            lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
            lv_obj_set_style_border_color(btn, COLOR_WHITE, 0);
            break;

        case CtrlBtnType::STOP:
            lv_label_set_text(icon, LV_SYMBOL_STOP);
            lv_obj_set_style_text_color(icon, COLOR_RED, 0);
            lv_obj_set_style_border_color(btn, COLOR_RED, 0);
            break;

        default:
            break;
    }
}

// ============================================
// Power Button
// ============================================
lv_obj_t* ui_widget_create_power_btn(lv_obj_t* parent) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 44, 32);
    lv_obj_add_style(btn, &style_button, 0);
    lv_obj_add_style(btn, &style_button_pressed, LV_STATE_PRESSED);

    lv_obj_t* icon = lv_label_create(btn);
    lv_label_set_text(icon, LV_SYMBOL_POWER);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(icon, COLOR_WHITE, 0);
    lv_obj_center(icon);

    return btn;
}

void ui_widget_update_power_btn(lv_obj_t* btn, bool is_on) {
    if (is_on) {
        lv_obj_set_style_bg_color(btn, COLOR_GREEN, 0);
        lv_obj_set_style_border_color(btn, COLOR_GREEN, 0);
    } else {
        lv_obj_set_style_bg_color(btn, COLOR_BUTTON_BG, 0);
        lv_obj_set_style_border_color(btn, COLOR_GRAY, 0);
    }
}

// ============================================
// Pressione prolungata (hold-to-confirm)
// ============================================
static void hold_bar_hide(HoldButton* hb) {
    if (hb->bar) {
        lv_obj_add_flag(hb->bar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_width(hb->bar, 0);
    }
}

static void hold_event_cb(lv_event_t* e) {
    HoldButton* hb = (HoldButton*)lv_event_get_user_data(e);
    lv_obj_t* btn = lv_event_get_current_target(e);

    switch (lv_event_get_code(e)) {
        case LV_EVENT_PRESSED:
            hb->pressed_armed = hb->armed && hb->armed();
            hb->fired = false;
            hb->start = lv_tick_get();
            if (hb->pressed_armed && hb->bar) {
                lv_obj_set_width(hb->bar, 0);
                lv_obj_clear_flag(hb->bar, LV_OBJ_FLAG_HIDDEN);
                lv_obj_move_foreground(hb->bar);
            }
            break;

        case LV_EVENT_PRESSING: {
            if (!hb->pressed_armed || hb->fired) break;
            uint32_t el = lv_tick_elaps(hb->start);
            uint32_t need = hb->hold_ms ? hb->hold_ms : HOLD_CONFIRM_MS;
            if (hb->bar) {
                lv_coord_t w = lv_obj_get_width(btn);
                lv_coord_t bw = (lv_coord_t)((int32_t)w * (int32_t)(el < need ? el : need) / (int32_t)need);
                lv_obj_set_width(hb->bar, bw);
            }
            if (el >= need) {
                hb->fired = true;
                hold_bar_hide(hb);
                if (hb->on_hold) hb->on_hold();
            }
            break;
        }

        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST:
            hold_bar_hide(hb);
            break;

        case LV_EVENT_CLICKED:
            // CLICKED arriva al rilascio anche dopo una pressione lunga: senza
            // questi controlli SPEGNI tenuto premuto riaccenderebbe il forno
            if (!hb->pressed_armed) {
                if (hb->on_tap) hb->on_tap();
            } else if (!hb->fired) {
                if (hb->on_short) hb->on_short();
            }
            break;

        default:
            break;
    }
}

void ui_widget_attach_hold(lv_obj_t* btn, HoldButton* hb) {
    // Barra di avanzamento sul bordo basso del pulsante
    hb->bar = lv_obj_create(btn);
    lv_obj_remove_style_all(hb->bar);
    lv_obj_set_size(hb->bar, 0, 6);
    lv_obj_align(hb->bar, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_add_flag(hb->bar, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(hb->bar, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(hb->bar, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(hb->bar, LV_OPA_80, 0);
    hb->fired = false;
    hb->pressed_armed = false;

    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_PRESSED, hb);
    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_PRESSING, hb);
    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_RELEASED, hb);
    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_PRESS_LOST, hb);
    lv_obj_add_event_cb(btn, hold_event_cb, LV_EVENT_CLICKED, hb);
}
