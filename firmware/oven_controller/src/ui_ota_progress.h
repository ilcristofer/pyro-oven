/**
 * Schermata di aggiornamento firmware in corso
 *
 * Compare a ogni upload OTA, da qualsiasi parte arrivi (pagina /ota, curl,
 * schermata Aggiorna Firmware). Durante l'upload loop() e' fermo dentro
 * handleClient(): la schermata si ridisegna da sola con lv_refr_now(),
 * chiamata dal gestore dell'upload
 */

#ifndef UI_OTA_PROGRESS_H
#define UI_OTA_PROGRESS_H

#include <stddef.h>

// Inizio upload: mostra la schermata (client = IP di chi carica)
void ui_ota_progress_begin(const char* client, bool was_on);

// Blocco ricevuto: aggiorna percentuale e dati (si ridisegna al massimo ~4 volte/s)
void ui_ota_progress_update(size_t received, size_t total);

// Fine upload: ok = firmware scritto e verificato, altrimenti msg e' l'errore
void ui_ota_progress_end(bool ok, const char* msg);

// Dal loop: chiude la schermata qualche secondo dopo un errore
void ui_ota_progress_tick();

#endif // UI_OTA_PROGRESS_H
