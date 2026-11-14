/**
 * Alphanumeric Keyboard
 * Tastiera per inserire nomi programmi (max 16 char)
 */

#ifndef UI_KEYBOARD_H
#define UI_KEYBOARD_H

#include <lvgl.h>

// Mostra tastiera alfanumerica
// target: buffer dove scrivere il risultato
// title: titolo da mostrare
// max_len: lunghezza massima (escluso null terminator)
void ui_keyboard_show(char* target, const char* title, uint8_t max_len);

// Nasconde tastiera
void ui_keyboard_hide();

// Controlla visibilità
bool ui_keyboard_is_visible();

#endif // UI_KEYBOARD_H
