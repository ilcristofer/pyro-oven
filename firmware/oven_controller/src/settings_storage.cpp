/**
 * Settings Storage Implementation
 * Usa ESP32 Preferences (NVS) per persistenza
 */

#include "settings_storage.h"
#include <Preferences.h>

// Namespace per le impostazioni
#define PREFS_NAMESPACE "forno"

// Chiavi NVS
#define KEY_SOUND_ENABLED "snd_on"
#define KEY_VOLUME        "volume"
#define KEY_TACH_ENABLED  "tach_on"
#define KEY_SIMPLE_TARGET "simp_tgt"
#define KEY_SIMPLE_TIMER  "simp_tmr"   // minuti: solo lettura (migrazione)
#define KEY_SIMPLE_TIMER_S "simp_tmrs" // secondi: il timer accetta anche 1:30
#define KEY_WIFI_ALWAYS   "wifi_on"
#define KEY_P0_TIMER_SEC  "p0_sec"    // secondi del timer di P0 (la fase e' in minuti)
#define KEY_SOAK_OVERRIDE "soak_ovr"
#define KEY_INITIALIZED   "init"

// Preferences instance
static Preferences s_prefs;

// Cached values (per evitare letture continue da flash)
static bool s_sound_enabled = DEFAULT_SOUND_ENABLED;
static uint8_t s_volume = DEFAULT_VOLUME;
static bool s_tach_enabled = DEFAULT_TACH_ENABLED;
static bool s_wifi_always_on = DEFAULT_WIFI_ALWAYS_ON;
static int s_soak_override = DEFAULT_SOAK_OVERRIDE;
static int s_simple_target = 200;
static int s_simple_timer = 0;
static int s_p0_timer_sec = 0;

// ============================================
// Initialize and load settings
// ============================================
void settings_init() {
    Serial.println("Settings: Caricamento da NVS...");

    s_prefs.begin(PREFS_NAMESPACE, true);  // true = read-only

    // Controlla se è la prima esecuzione
    bool initialized = s_prefs.getBool(KEY_INITIALIZED, false);

    if (initialized) {
        // Carica impostazioni salvate
        s_sound_enabled = s_prefs.getBool(KEY_SOUND_ENABLED, DEFAULT_SOUND_ENABLED);
        s_volume = s_prefs.getUChar(KEY_VOLUME, DEFAULT_VOLUME);
        s_tach_enabled = s_prefs.getBool(KEY_TACH_ENABLED, DEFAULT_TACH_ENABLED);
        s_wifi_always_on = s_prefs.getBool(KEY_WIFI_ALWAYS, DEFAULT_WIFI_ALWAYS_ON);
        s_soak_override = s_prefs.getInt(KEY_SOAK_OVERRIDE, DEFAULT_SOAK_OVERRIDE);
        s_simple_target = s_prefs.getUShort(KEY_SIMPLE_TARGET, 200);
        if (s_prefs.isKey(KEY_SIMPLE_TIMER_S)) {
            s_simple_timer = s_prefs.getUShort(KEY_SIMPLE_TIMER_S, 0);
        } else {
            s_simple_timer = s_prefs.getUShort(KEY_SIMPLE_TIMER, 0) * 60;
        }
        if (s_simple_timer > 99 * 60 + 59) s_simple_timer = 0;
        s_p0_timer_sec = s_prefs.getUChar(KEY_P0_TIMER_SEC, 0);
        if (s_p0_timer_sec > 59) s_p0_timer_sec = 0;
        if (s_simple_target < 50 || s_simple_target > 500) s_simple_target = 200;
        Serial.printf("Settings: Caricate (sound=%s, volume=%d%%, tach=%s, wifi=%s, simple=%d/%ds)\n",
                     s_sound_enabled ? "ON" : "OFF", s_volume,
                     s_tach_enabled ? "ON" : "OFF",
                     s_wifi_always_on ? "ON" : "OFF",
                     s_simple_target, s_simple_timer);
    } else {
        // Prima esecuzione - usa default
        Serial.println("Settings: Prima esecuzione, uso default");
        s_sound_enabled = DEFAULT_SOUND_ENABLED;
        s_volume = DEFAULT_VOLUME;
        s_tach_enabled = DEFAULT_TACH_ENABLED;
        s_wifi_always_on = DEFAULT_WIFI_ALWAYS_ON;
        s_soak_override = DEFAULT_SOAK_OVERRIDE;
        s_simple_target = 200;
        s_simple_timer = 0;
    }

    s_prefs.end();

    // Se prima esecuzione, salva i default
    if (!initialized) {
        settings_save();
        // Marca come inizializzato
        s_prefs.begin(PREFS_NAMESPACE, false);
        s_prefs.putBool(KEY_INITIALIZED, true);
        s_prefs.end();
    }
}

// ============================================
// Save all settings
// ============================================
void settings_save() {
    s_prefs.begin(PREFS_NAMESPACE, false);  // false = read/write

    s_prefs.putBool(KEY_SOUND_ENABLED, s_sound_enabled);
    s_prefs.putUChar(KEY_VOLUME, s_volume);
    s_prefs.putBool(KEY_TACH_ENABLED, s_tach_enabled);
    s_prefs.putBool(KEY_WIFI_ALWAYS, s_wifi_always_on);
    s_prefs.putInt(KEY_SOAK_OVERRIDE, s_soak_override);
    s_prefs.putUShort(KEY_SIMPLE_TARGET, (uint16_t)s_simple_target);
    s_prefs.putUShort(KEY_SIMPLE_TIMER_S, (uint16_t)s_simple_timer);
    s_prefs.putUChar(KEY_P0_TIMER_SEC, (uint8_t)s_p0_timer_sec);

    s_prefs.end();

    Serial.println("Settings: Salvate su NVS");
}

// ============================================
// Factory reset
// ============================================
void settings_factory_reset() {
    Serial.println("Settings: RESET DI FABBRICA");

    // Cancella tutto il namespace
    s_prefs.begin(PREFS_NAMESPACE, false);
    s_prefs.clear();
    s_prefs.end();

    // Reimposta ai valori di default
    s_sound_enabled = DEFAULT_SOUND_ENABLED;
    s_volume = DEFAULT_VOLUME;
    s_tach_enabled = DEFAULT_TACH_ENABLED;
    s_wifi_always_on = DEFAULT_WIFI_ALWAYS_ON;
    s_soak_override = DEFAULT_SOAK_OVERRIDE;
    s_simple_target = 200;
    s_simple_timer = 0;
    s_p0_timer_sec = 0;

    // Salva i default
    settings_save();

    // Marca come inizializzato
    s_prefs.begin(PREFS_NAMESPACE, false);
    s_prefs.putBool(KEY_INITIALIZED, true);
    s_prefs.end();

    Serial.println("Settings: Reset completato");
}

// ============================================
// Audio settings
// ============================================
bool settings_get_sound_enabled() {
    return s_sound_enabled;
}

void settings_set_sound_enabled(bool enabled) {
    if (s_sound_enabled != enabled) {
        s_sound_enabled = enabled;
        settings_save();
    }
}

uint8_t settings_get_volume() {
    return s_volume;
}

void settings_set_volume(uint8_t volume) {
    if (volume > 100) volume = 100;
    if (s_volume != volume) {
        s_volume = volume;
        settings_save();
    }
}

// ============================================
// TACH settings
// ============================================
bool settings_get_tach_enabled() {
    return s_tach_enabled;
}

void settings_set_tach_enabled(bool enabled) {
    if (s_tach_enabled != enabled) {
        s_tach_enabled = enabled;
        settings_save();
    }
}

// ============================================
// WiFi settings
// ============================================
int settings_get_soak_override() {
    return s_soak_override;
}

void settings_set_soak_override(int minutes) {
    if (minutes < 0) minutes = 0;
    if (minutes > SOAK_OVERRIDE_MAX) minutes = SOAK_OVERRIDE_MAX;
    s_soak_override = minutes;
    settings_save();
}

bool settings_get_wifi_always_on() {
    return s_wifi_always_on;
}

void settings_set_wifi_always_on(bool enabled) {
    if (s_wifi_always_on != enabled) {
        s_wifi_always_on = enabled;
        settings_save();
    }
}

// ============================================
// SEMPLICE mode settings
// ============================================
void settings_save_simple_target(int target) {
    if (s_simple_target != target) {
        s_simple_target = target;
        settings_save();
    }
}

int settings_load_simple_target() {
    return s_simple_target;
}

void settings_save_simple_timer_s(int seconds) {
    if (s_simple_timer != seconds) {
        s_simple_timer = seconds;
        settings_save();
    }
}

int settings_load_simple_timer_s() {
    return s_simple_timer;
}

void settings_save_p0_timer_sec(int seconds) {
    if (seconds < 0 || seconds > 59) seconds = 0;
    if (s_p0_timer_sec != seconds) {
        s_p0_timer_sec = seconds;
        settings_save();
    }
}

int settings_load_p0_timer_sec() {
    return s_p0_timer_sec;
}
