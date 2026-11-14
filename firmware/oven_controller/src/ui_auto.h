/**
 * Automatic Mode Main Screen
 * Simplified interface with preset selection, single temperature, timer
 */

#ifndef UI_AUTO_H
#define UI_AUTO_H

#include <lvgl.h>

void ui_auto_create();
void ui_auto_destroy();
void ui_auto_update();
lv_obj_t* ui_auto_get_screen();
bool ui_auto_is_visible();

// Azioni della modalita' SEMPLICE, condivise da touch e web (nessuna chiamata
// LVGL: sicure anche da un handler HTTP). Tenerle in un solo posto evita che
// il web si comporti diversamente dal touch
void ui_auto_set_target(int target);   // temperatura unica, salvata in NVS
void ui_auto_power_on();               // ACCENDI: target SEMPLICE + preriscaldo
void ui_auto_timer_play();             // avvia / pausa / riprende il timer
void ui_auto_timer_reset();            // ferma e torna all'ultimo tempo usato
void ui_auto_toggle_boost(bool cielo); // boost senza countdown

// Taratura SEMPLICE, per web e app: temperatura scelta e scostamento del
// cielo (temperatura mostrata = cielo - ui_auto_offset_cielo(target))
int ui_auto_get_target();
int ui_auto_offset_cielo(int target);
// Scostamento del cielo mostrato adesso: cresce da 0 (forno freddo) allo
// scostamento pieno quando il cielo arriva al suo obiettivo
int ui_auto_shown_offset_cielo();
// Temperatura grande di SEMPLICE a forno acceso (-1000 = entrambe le sonde in errore)
int ui_auto_display_temp();

#endif // UI_AUTO_H
