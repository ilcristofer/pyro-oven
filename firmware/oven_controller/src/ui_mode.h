/**
 * UI Mode Switching (SEMPLICE <-> PRO)
 *
 * Il cambio modalita' distrugge e ricrea uno schermo LVGL: eseguirlo dentro
 * una callback dell'oggetto che sta per essere cancellato (o dentro un handler
 * HTTP) fa crashare LVGL. Le richieste vengono quindi accodate ed eseguite
 * da ui_mode_process_pending() nel main loop.
 */

#ifndef UI_MODE_H
#define UI_MODE_H

#include <lvgl.h>

// Durata pressione sul pulsante MENU per cambiare modalita' (ms)
#define UI_MODE_LONG_PRESS_MS  1000

// Accoda un cambio modalita' (sicuro da callback LVGL e handler web)
void ui_mode_request_switch(bool to_pro);
void ui_mode_request_toggle();

// Esegue il cambio accodato - chiamare SOLO dal main loop
void ui_mode_process_pending();

// true quando lo schermo attivo e' quello PRO
bool ui_mode_is_pro();

// Aggiunge il comportamento "tieni premuto per cambiare modalita'" a un
// pulsante MENU (funziona sia su ui_main che su ui_auto)
void ui_mode_attach_menu_long_press(lv_obj_t* btn);

// true (una sola volta) se l'ultima pressione sul MENU era lunga: permette
// all'handler del click di non aprire le impostazioni
bool ui_mode_consume_long_press();

#endif // UI_MODE_H
