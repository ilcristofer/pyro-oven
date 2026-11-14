/**
 * Settings Screen
 * Forno Pizza Project
 */

#ifndef UI_SETTINGS_H
#define UI_SETTINGS_H

#include <lvgl.h>

/**
 * Apre le impostazioni dalla home: sempre sull'elenco principale
 */
void ui_settings_open();

/**
 * Mostra la schermata impostazioni sulla pagina in cui era (ritorno da
 * WiFi, aggiornamento firmware, programmi)
 */
void ui_settings_show();

/**
 * Nasconde la schermata impostazioni e torna alla main
 */
void ui_settings_hide();

/**
 * Verifica se la schermata settings è visibile
 */
bool ui_settings_is_visible();

/**
 * Torna alla schermata impostazioni (dal tastierino aperto da li')
 */
void ui_settings_return();

/**
 * Aggiorna elementi dinamici (RPM ventola)
 * Chiamare dal loop principale
 */
void ui_settings_update();

#endif // UI_SETTINGS_H
