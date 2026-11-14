/**
 * Program Edit Screen
 * Schermata per modificare i parametri di un programma
 */

#ifndef UI_PROGRAM_EDIT_H
#define UI_PROGRAM_EDIT_H

#include <lvgl.h>

// Mostra schermata modifica programma
void ui_program_edit_show(uint8_t program_index);

// Nasconde schermata e torna a selezione programmi
void ui_program_edit_hide();

// Controlla se la schermata è visibile
bool ui_program_edit_is_visible();

// Aggiorna display (chiamato dopo keyboard/keypad)
void ui_program_edit_refresh();

#endif // UI_PROGRAM_EDIT_H
