/**
 * Programs Storage - Gestione persistenza programmi in NVS
 * I programmi sopravvivono agli aggiornamenti firmware
 */

#ifndef PROGRAMS_STORAGE_H
#define PROGRAMS_STORAGE_H

#include <Arduino.h>
#include "oven_state.h"

// ============================================
// Funzioni pubbliche
// ============================================

// Inizializza storage programmi (carica da NVS o crea default)
void programs_storage_init();

// Carica un programma da NVS in g_programs[index]
bool programs_load(uint8_t index);

// Salva un programma da g_programs[index] su NVS
bool programs_save(uint8_t index);

// Ottiene indice programma correntemente selezionato
uint8_t programs_get_current();

// Imposta programma corrente e carica in g_state
void programs_set_current(uint8_t index);

// Carica un programma nelle variabili operative di g_state
void programs_load_to_state(uint8_t index);

// Reset singolo programma ai valori di fabbrica
void programs_reset_single(uint8_t index);

// Reset tutti i programmi ai valori di fabbrica
void programs_factory_reset();

// Ottiene descrizione breve del programma (es. "3F 155/155 2:00")
void programs_get_description(uint8_t index, char* buf, size_t len);

// Verifica se il programma ha contenuto (non vuoto)
bool programs_is_configured(uint8_t index);

// Salva i parametri correnti di g_state in P0 (Manuale)
// Chiamare quando l'utente modifica parametri dalla main screen
void programs_save_manual_params();

// Riporta nome e valori di un programma negli intervalli ammessi
void programs_sanitize(Program* p);

// Salva i parametri correnti della fase attiva nel programma caricato
// Chiamare dal pulsante save sulla main screen
void programs_save_current_phase_params();

// Costruisce JSON array di tutti i 20 programmi nel buffer fornito
// Ritorna lunghezza scritta (0 se errore)
size_t programs_build_json(char* buf, size_t buf_size);

#endif // PROGRAMS_STORAGE_H
