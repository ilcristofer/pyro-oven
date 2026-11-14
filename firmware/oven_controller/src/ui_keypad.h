/**
 * Numeric Keypad UI
 * Full-screen numeric input
 */

#ifndef UI_KEYPAD_H
#define UI_KEYPAD_H

#include <lvgl.h>

// Show numeric keypad
// target: pointer to variable to edit
// title: title to display
// min_val: minimum allowed value
// max_val: maximum allowed value
// unit: suffisso mostrato accanto al valore (nullptr = dedotto dal range)
// C a campo vuoto annulla; un valore fuori range viene rifiutato
void ui_keypad_show(int* target, const char* title, int min_val, int max_val,
                    const char* unit = nullptr);

// Tastierino per un tempo m:ss (timer). Si digita come sul microonde: le
// ultime due cifre sono i secondi. Mostra anche i preset delle pizze
void ui_keypad_show_time(int* minutes, int* seconds, const char* title);

// Tastierino per un'ora del giorno hh:mm (stessa digitazione, senza preset):
// "700" = 07:00
void ui_keypad_show_clock(int* hour, int* minute, const char* title);

// Hide keypad and return to main screen
void ui_keypad_hide();

// Check if keypad is visible
bool ui_keypad_is_visible();

#endif // UI_KEYPAD_H
