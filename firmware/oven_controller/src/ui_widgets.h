/**
 * UI Custom Widgets - Effeuno style
 * Phase gauge, temperature bar, resistance icon, function buttons
 */

#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H

#include <lvgl.h>
#include "oven_state.h"

// ============================================
// Phase Gauge Widget
// Circular gauge with ticks showing phase info
// ============================================

// Create a phase gauge (circular arc with ticks)
lv_obj_t* ui_widget_create_phase_gauge(lv_obj_t* parent, int phase_index);

// Update phase gauge with current values
void ui_widget_update_phase_gauge(lv_obj_t* gauge, int phase_index, bool is_active, int progress);

// ============================================
// Temperature Bar Widget
// Vertical bar with gradient (red-yellow-green)
// ============================================

// Create vertical temperature bar
lv_obj_t* ui_widget_create_temp_bar(lv_obj_t* parent);

// Update temperature bar
void ui_widget_update_temp_bar(lv_obj_t* bar, int current_temp, int target_temp);

// ============================================
// Resistance Icon Widget
// Zigzag symbol that indicates heating state
// ============================================

// Create resistance icon (zigzag)
lv_obj_t* ui_widget_create_resistance_icon(lv_obj_t* parent);

// Update resistance icon state
void ui_widget_update_resistance_icon(lv_obj_t* icon, bool is_on, bool blink_state);

// ============================================
// SET Icon Widget
// Icon showing cielo (3 lines) or platea (1 line) set-point
// ============================================

// Create SET icon for cielo (3 horizontal lines)
lv_obj_t* ui_widget_create_set_icon_cielo(lv_obj_t* parent);

// Create SET icon for platea (1 horizontal line)
lv_obj_t* ui_widget_create_set_icon_platea(lv_obj_t* parent);

// ============================================
// Status Bar Widget
// Colored bar showing oven status
// ============================================

// Create status bar
lv_obj_t* ui_widget_create_status_bar(lv_obj_t* parent);

// Update status bar based on state
void ui_widget_update_status_bar(lv_obj_t* bar, OvenMode mode, bool timer_expired, bool blink);

// ============================================
// Function Button Widget
// Button with icon for the 2x4 grid
// ============================================

// Function button types (for icons)
enum class FuncBtnType {
    LIGHT,          // Lampadina (luce camera)
    BOOST_CIELO,    // Resistenza zigzag (boost cielo)
    PREHEAT,        // Termometro con grafico (preriscaldamento)
    BOOST_PLATEA,   // Termometro con grafico (boost platea)
    PROGRAMS,       // P (selezione programmi)
    MAINTAIN,       // m con resistenza (mantenimento)
    POWER,          // Power ON/OFF
    SERVICE,        // Servizio pizze (infornate in sequenza)
    MENU            // 3 linee orizzontali (menu impostazioni)
};

// Create function button with specific icon
lv_obj_t* ui_widget_create_func_btn(lv_obj_t* parent, FuncBtnType type);

// Update function button state (active/inactive)
void ui_widget_update_func_btn(lv_obj_t* btn, FuncBtnType type, bool active);

// Set function button enabled/disabled state (disabled = light gray, no click)
void ui_widget_set_func_btn_enabled(lv_obj_t* btn, FuncBtnType type, bool enabled);

// ============================================
// Control Button Widget
// Button in the controls row (reset, play, save)
// ============================================

// Control button types
enum class CtrlBtnType {
    RESTART,        // Freccia circolare (arancione)
    PLAY,           // Triangolo play (verde)
    PAUSE,          // Due barre verticali (bianco)
    STOP,           // Quadrato (rosso)
    SAVE            // Floppy disk (bianco)
};

// Create control button with specific icon
lv_obj_t* ui_widget_create_ctrl_btn(lv_obj_t* parent, CtrlBtnType type);

// Update control button state
void ui_widget_update_ctrl_btn(lv_obj_t* btn, CtrlBtnType type);

// ============================================
// Power Button Widget
// ON/OFF button in header
// ============================================

// Create power button
lv_obj_t* ui_widget_create_power_btn(lv_obj_t* parent);

// Update power button state
void ui_widget_update_power_btn(lv_obj_t* btn, bool is_on);

// ============================================
// Pressione prolungata per le azioni da non fare per sbaglio (SPEGNI)
// Quando armed() e' vero il pulsante va tenuto premuto HOLD_CONFIRM_MS: una
// barra dentro il pulsante mostra l'avanzamento. Altrimenti e' un tocco
// normale. Le callback non ricevono l'evento: sono semplici azioni.
// ============================================
#define HOLD_CONFIRM_MS 1000

struct HoldButton {
    bool (*armed)();      // true = serve la pressione lunga
    void (*on_tap)();     // tocco quando non armato
    void (*on_hold)();    // pressione lunga completata
    void (*on_short)();   // armato ma rilasciato troppo presto
    // Stato interno (gestito da ui_widget_attach_hold)
    lv_obj_t* bar;
    uint32_t start;
    bool pressed_armed;
    bool fired;
    uint16_t hold_ms;     // durata della pressione (0 = HOLD_CONFIRM_MS)
};

// Collega il comportamento al pulsante (sostituisce il suo LV_EVENT_CLICKED).
// hb deve restare valido finche' esiste il pulsante (static nel chiamante).
void ui_widget_attach_hold(lv_obj_t* btn, HoldButton* hb);

#endif // UI_WIDGETS_H
