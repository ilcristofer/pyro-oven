/**
 * Buzzer Driver Implementation
 * Uses ESP32 LEDC for PWM tone generation
 */

#include "buzzer.h"
#include "hardware_pins.h"
#include "settings_storage.h"

// LEDC configuration
#define BUZZER_LEDC_CHANNEL  0
#define BUZZER_LEDC_TIMER    0
#define BUZZER_LEDC_RESOLUTION 8  // 8-bit resolution

// Pattern types for complex sounds
enum BuzzerPattern {
    PATTERN_NONE,
    PATTERN_SINGLE,
    PATTERN_DOUBLE,
    PATTERN_ALERT,
    PATTERN_ERROR,
    PATTERN_MELODY
};

// Melody note: freq=0 means pause
struct MelodyNote {
    uint16_t freq;
    uint16_t dur;
};

// State machine
static bool s_enabled = true;
static bool s_playing = false;
static BuzzerPattern s_pattern = PATTERN_NONE;
static uint8_t s_pattern_step = 0;
static unsigned long s_tone_start = 0;
static unsigned long s_tone_duration = 0;
static unsigned long s_pause_until = 0;
static uint8_t s_error_count = 0;
static uint8_t s_volume = BUZZER_VOLUME_DEFAULT;

// Melody playback state
static const MelodyNote* s_melody_data = nullptr;
static uint16_t s_melody_len = 0;
static uint16_t s_melody_index = 0;
static unsigned long s_melody_start = 0;
static uint16_t s_melody_duty = 0;
static uint16_t s_cached_duty = 0;

// Forward declarations
static void buzzer_play(uint16_t freq);
static void buzzer_silent();

static void buzzer_recalc_duty() {
    if (s_volume == 0) { s_cached_duty = 0; return; }
    float v = s_volume / 100.0f;
    s_cached_duty = (uint16_t)(512.0f * v * v);
    if (s_cached_duty < 2) s_cached_duty = 2;
}

void buzzer_init() {
    // Configure LEDC for PWM
    ledcSetup(BUZZER_LEDC_CHANNEL, BUZZER_FREQ_MID, BUZZER_LEDC_RESOLUTION);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);  // Start silent

    // Carica impostazioni da NVS
    s_enabled = settings_get_sound_enabled();
    s_volume = settings_get_volume();
    buzzer_recalc_duty();

    s_playing = false;
    s_pattern = PATTERN_NONE;

    Serial.printf("Buzzer: enabled=%s, volume=%d%%\n",
                 s_enabled ? "ON" : "OFF", s_volume);
}

void buzzer_update() {
    if (!s_enabled) return;

    unsigned long now = millis();

    // Melody playback (independent from other patterns)
    if (s_pattern == PATTERN_MELODY && s_melody_data != nullptr) {
        if (now - s_melody_start >= s_melody_data[s_melody_index].dur) {
            s_melody_index++;
            if (s_melody_index >= s_melody_len) {
                // Melody complete
                buzzer_silent();
                s_pattern = PATTERN_NONE;
                s_playing = false;
                s_melody_data = nullptr;
            } else {
                // Play next note/pause
                if (s_melody_data[s_melody_index].freq > 0) {
                    ledcSetup(BUZZER_LEDC_CHANNEL, s_melody_data[s_melody_index].freq, 10);
                    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
                    ledcWrite(BUZZER_LEDC_CHANNEL, s_melody_duty);
                } else {
                    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
                }
                s_melody_start = now;
            }
        }
        return;  // Don't process other patterns while melody plays
    }

    // Check if we're in a pause between tones
    if (s_pause_until > 0) {
        if (now < s_pause_until) return;
        s_pause_until = 0;
        s_pattern_step++;  // Avanza dopo la pausa
    }

    // Check if current tone has finished
    if (s_playing && (now - s_tone_start >= s_tone_duration)) {
        buzzer_silent();
        s_playing = false;
        s_pattern_step++;
    }

    // Process pattern state machine
    if (!s_playing && s_pattern != PATTERN_NONE) {
        switch (s_pattern) {
            case PATTERN_SINGLE:
                // Single beep complete
                s_pattern = PATTERN_NONE;
                break;

            case PATTERN_DOUBLE:
                if (s_pattern_step == 1) {
                    // Pause between beeps
                    s_pause_until = now + 80;
                } else if (s_pattern_step == 2) {
                    // Second beep
                    buzzer_play(BUZZER_FREQ_MID);
                    s_tone_start = now;
                    s_tone_duration = BUZZER_DURATION_SHORT;
                    s_playing = true;
                } else {
                    s_pattern = PATTERN_NONE;
                }
                break;

            case PATTERN_ALERT:
                // Repeat alert 6 times (1000ms tone + 500ms pause = ~8.5s)
                if (s_pattern_step < 12) {
                    if (s_pattern_step % 2 == 0) {
                        // Tone
                        buzzer_play(BUZZER_FREQ_ALERT);
                        s_tone_start = now;
                        s_tone_duration = 1000;
                        s_playing = true;
                    } else {
                        // Pause
                        s_pause_until = now + 500;
                    }
                } else {
                    s_pattern = PATTERN_NONE;
                }
                break;

            case PATTERN_ERROR:
                if (s_error_count < 5) {
                    if (s_pattern_step % 2 == 0) {
                        // Beep
                        buzzer_play(BUZZER_FREQ_HIGH);
                        s_tone_start = now;
                        s_tone_duration = BUZZER_DURATION_MEDIUM;
                        s_playing = true;
                    } else {
                        // Pause
                        s_pause_until = now + 100;
                        s_error_count++;
                    }
                } else {
                    // Loop: pausa lunga e ricomincia (ripete all'infinito fino a buzzer_stop)
                    s_pause_until = now + 2000;
                    s_error_count = 0;
                    s_pattern_step = 0;
                }
                break;

            default:
                break;
        }
    }
}

void buzzer_startup() {
    if (!s_enabled) return;

    // Tre toni in crescendo (triade maggiore C-E-G)
    // Primo tono: C5 (523Hz)
    ledcSetup(BUZZER_LEDC_CHANNEL, 523, 10);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, s_cached_duty);
    delay(120);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
    delay(50);

    // Secondo tono: E5 (659Hz)
    ledcSetup(BUZZER_LEDC_CHANNEL, 659, 10);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, s_cached_duty);
    delay(120);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
    delay(50);

    // Terzo tono: G5 (784Hz)
    ledcSetup(BUZZER_LEDC_CHANNEL, 784, 10);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
    ledcWrite(BUZZER_LEDC_CHANNEL, s_cached_duty);
    delay(200);
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
}

// L'amour Toujours melody data (static const = flash, not stack)
// Tonalita': Re maggiore (D major), ~138 BPM
// N = nota breve (~140ms), L = nota lunga (~210ms), P = pausa breve, PL = pausa lunga
#define DUR_SHORT 140
#define DUR_LONG  210
#define DUR_GAP   25
#define DUR_GAPL  60
static const MelodyNote lamour_melody[] = {
    // Riff intro
    // fa#, fa# RE DO#, DO#, DO# RE si
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_B4,  DUR_LONG},  {0, DUR_GAPL},
    // si, si la si, si la si la fa#
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs4, DUR_LONG},  {0, DUR_GAPL},
    // fa#, fa# RE DO#, DO#, DO# RE si
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_B4,  DUR_LONG},  {0, DUR_GAPL},
    // si, si la si, si la si la si la fa#
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_B4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A4,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs4, DUR_LONG},  {0, DUR_GAPL},

    // Ritornello
    // re, mi fa# do#, re mi re
    {NOTE_D5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_E5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_LONG},  {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_E5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAPL},
    // sol, fa# sol la, sol fa# mi, fa# la fa#
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_E5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_LONG},  {0, DUR_GAPL},
    // sol, fa# mi re, mi fa# do#, re mi re
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_E5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_E5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_LONG},  {0, DUR_GAP},
    {NOTE_Cs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_E5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAPL},
    // sol, fa# sol la, sol fa# mi, fa# la fa#
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_E5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_LONG},  {0, DUR_GAPL},

    // Finale
    // sol, fa# mi, re, do#, re
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_LONG},  {0, DUR_GAP},
    {NOTE_E5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_Cs5, DUR_LONG},  {0, DUR_GAP},
    {NOTE_D5,  280},        {0, DUR_GAPL},
    // sol, fa# mi, re, mi, fa#
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_LONG},  {0, DUR_GAP},
    {NOTE_E5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_D5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_E5,  DUR_LONG},  {0, DUR_GAP},
    {NOTE_Fs5, 280},        {0, DUR_GAPL},
    // sol, fa# sol la....
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_Fs5, DUR_SHORT}, {0, DUR_GAP},
    {NOTE_G5,  DUR_SHORT}, {0, DUR_GAP},
    {NOTE_A5,  500},
};
#undef DUR_SHORT
#undef DUR_LONG
#undef DUR_GAP
#undef DUR_GAPL

void buzzer_lamour_toujours() {
    if (!s_enabled) return;

    // Calcola duty per volume corrente
    float v = s_volume / 100.0f;
    s_melody_duty = (uint16_t)(512.0f * v * v);
    if (s_melody_duty < 2) s_melody_duty = 2;

    // Configura state machine per playback non-bloccante
    s_melody_data = lamour_melody;
    s_melody_len = sizeof(lamour_melody) / sizeof(lamour_melody[0]);
    s_melody_index = 0;
    s_melody_start = millis();
    s_pattern = PATTERN_MELODY;
    s_playing = true;

    // Avvia prima nota
    if (lamour_melody[0].freq > 0) {
        ledcSetup(BUZZER_LEDC_CHANNEL, lamour_melody[0].freq, 10);
        ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);
        ledcWrite(BUZZER_LEDC_CHANNEL, s_melody_duty);
    }
}

void buzzer_beep() {
    if (!s_enabled) return;

    s_pattern = PATTERN_SINGLE;
    s_pattern_step = 0;

    buzzer_play(BUZZER_FREQ_MID);
    s_tone_start = millis();
    s_tone_duration = BUZZER_DURATION_SHORT;
    s_playing = true;
}

void buzzer_confirm() {
    if (!s_enabled) return;

    s_pattern = PATTERN_DOUBLE;
    s_pattern_step = 0;

    // First beep
    buzzer_play(BUZZER_FREQ_MID);
    s_tone_start = millis();
    s_tone_duration = BUZZER_DURATION_SHORT;
    s_playing = true;
}

void buzzer_alert() {
    if (!s_enabled) return;

    s_pattern = PATTERN_ALERT;
    s_pattern_step = 0;

    buzzer_play(BUZZER_FREQ_ALERT);
    s_tone_start = millis();
    s_tone_duration = 1000;
    s_playing = true;
}

void buzzer_error() {
    if (!s_enabled) return;

    s_pattern = PATTERN_ERROR;
    s_pattern_step = 0;
    s_error_count = 0;

    // First beep
    buzzer_play(BUZZER_FREQ_HIGH);
    s_tone_start = millis();
    s_tone_duration = BUZZER_DURATION_MEDIUM;
    s_playing = true;
}

void buzzer_tone(uint16_t freq, uint16_t duration) {
    if (!s_enabled) return;

    s_pattern = PATTERN_SINGLE;
    s_pattern_step = 0;

    buzzer_play(freq);
    s_tone_start = millis();
    s_tone_duration = duration;
    s_playing = true;
}

void buzzer_stop() {
    buzzer_silent();
    s_playing = false;
    s_pattern = PATTERN_NONE;
    s_pause_until = 0;
    s_melody_data = nullptr;
}

void buzzer_set_enabled(bool enabled) {
    s_enabled = enabled;
    settings_set_sound_enabled(enabled);  // Salva su NVS
    if (!enabled) {
        buzzer_stop();
    }
}

bool buzzer_is_enabled() {
    return s_enabled;
}

void buzzer_set_volume(uint8_t volume) {
    if (volume < BUZZER_VOLUME_MIN) volume = BUZZER_VOLUME_MIN;
    if (volume > BUZZER_VOLUME_MAX) volume = BUZZER_VOLUME_MAX;
    s_volume = volume;
    buzzer_recalc_duty();
    settings_set_volume(volume);  // Salva su NVS
}

uint8_t buzzer_get_volume() {
    return s_volume;
}

// ============================================
// Private functions
// ============================================

static void buzzer_play(uint16_t freq) {
    // Configura timer con frequenza desiderata (10-bit = 0-1023)
    // NON usare ledcWriteTone: imposta duty 50% internamente e il successivo
    // ledcWrite potrebbe non sovrascriverlo su alcune versioni Arduino-ESP32
    ledcSetup(BUZZER_LEDC_CHANNEL, freq, 10);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CHANNEL);

    ledcWrite(BUZZER_LEDC_CHANNEL, s_cached_duty);
}

static void buzzer_silent() {
    ledcWrite(BUZZER_LEDC_CHANNEL, 0);
}
