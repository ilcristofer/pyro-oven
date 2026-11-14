/**
 * Settings Storage - Persistenza impostazioni con NVS
 * I dati sopravvivono agli aggiornamenti firmware
 */

#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include <Arduino.h>

// ============================================
// Valori di default (fabbrica)
// ============================================
#define DEFAULT_SOUND_ENABLED   true
#define DEFAULT_VOLUME          70
#define DEFAULT_TACH_ENABLED    true    // Sicurezza ventola attiva (GPIO5)
#define DEFAULT_WIFI_ALWAYS_ON  true    // WiFi sempre attivo per controllo remoto
#define DEFAULT_SOAK_OVERRIDE   0       // 0 = attesa pietra automatica (appresa)
#define SOAK_OVERRIDE_MAX       45      // min

// ============================================
// Funzioni pubbliche
// ============================================

// Inizializza e carica impostazioni da NVS
void settings_init();

// Salva tutte le impostazioni su NVS
void settings_save();

// Reset alle impostazioni di fabbrica
void settings_factory_reset();

// --- Audio ---
bool settings_get_sound_enabled();
void settings_set_sound_enabled(bool enabled);

uint8_t settings_get_volume();
void settings_set_volume(uint8_t volume);

// --- TACH ventola ---
bool settings_get_tach_enabled();
void settings_set_tach_enabled(bool enabled);

// --- WiFi ---
bool settings_get_wifi_always_on();
void settings_set_wifi_always_on(bool enabled);

// --- Attesa pietra (soak) ---
// 0 = automatica (il forno la impara), >0 = minuti forzati dall'utente
int  settings_get_soak_override();
void settings_set_soak_override(int minutes);

// --- Modalita' SEMPLICE ---
void settings_save_simple_target(int target);   // Salva ultima temperatura
int  settings_load_simple_target();              // Carica (default 200)
void settings_save_simple_timer_s(int seconds);  // Salva ultimo timer (secondi)
int  settings_load_simple_timer_s();             // Carica (default 0)

// --- P0 (MANUALE) ---
// La durata delle fasi e' in minuti interi (struct Phase invariata per la
// compatibilita' NVS): i secondi del timer di P0 (1:30) si salvano qui
void settings_save_p0_timer_sec(int seconds);
int  settings_load_p0_timer_sec();

#endif // SETTINGS_STORAGE_H
