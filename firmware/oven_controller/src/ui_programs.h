/**
 * Programs Selection Screen
 * Schermata per selezionare o gestire i programmi
 */

#ifndef UI_PROGRAMS_H
#define UI_PROGRAMS_H

#include <lvgl.h>

// Mostra schermata selezione programmi (tap = seleziona e torna)
void ui_programs_show();

// Mostra schermata gestione programmi (tap = apre editor)
void ui_programs_show_manage();

// Nasconde schermata e torna a main o settings
void ui_programs_hide();

// Controlla se la schermata è visibile
bool ui_programs_is_visible();

// Aggiorna lista (dopo modifica programma)
void ui_programs_refresh();

// Distrugge la schermata per liberare memoria (chiamata prima di aprire editor)
void ui_programs_destroy();

#endif // UI_PROGRAMS_H
