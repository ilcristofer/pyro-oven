/**
 * Avvio programmato - implementazione
 */

#include "schedule.h"
#include <Preferences.h>
#include <time.h>
#include "oven_state.h"
#include "ui_auto.h"
#include "wifi_time.h"
#include "settings_storage.h"
#include "ssr_control.h"
#include "buzzer.h"

#define PREFS_NAMESPACE "sched"
#define KEY_ARMED  "on"
#define KEY_HOUR   "h"
#define KEY_MINUTE "m"
#define KEY_TEMP   "t"
#define KEY_DUR    "d"
#define KEY_AT     "at"    // epoch dell'avvio (sopravvive a un riavvio)

static Preferences s_prefs;

static ScheduleState s_state = ScheduleState::OFF;
static int s_hour = SCHEDULE_DEFAULT_HOUR;
static int s_minute = SCHEDULE_DEFAULT_MINUTE;
static int s_temp = 200;
static int s_dur_s = SCHEDULE_DEFAULT_DUR_S;
static uint32_t s_start_epoch = 0;
static unsigned long s_heat_start_ms = 0;
static unsigned long s_last_check_ms = 0;
static char s_note[40] = "";

// ============================================
// Persistenza
// Si salva solo ARMED: HEATING e TIMING non sopravvivono a un riavvio (il
// forno riparte spento), e un avvio gia' scattato non deve ripetersi
// ============================================
static void save() {
    s_prefs.begin(PREFS_NAMESPACE, false);
    s_prefs.putBool(KEY_ARMED, s_state == ScheduleState::ARMED);
    s_prefs.putUChar(KEY_HOUR, (uint8_t)s_hour);
    s_prefs.putUChar(KEY_MINUTE, (uint8_t)s_minute);
    s_prefs.putUShort(KEY_TEMP, (uint16_t)s_temp);
    s_prefs.putUShort(KEY_DUR, (uint16_t)s_dur_s);
    s_prefs.putULong(KEY_AT, s_start_epoch);
    s_prefs.end();
}

void schedule_init() {
    s_prefs.begin(PREFS_NAMESPACE, true);
    bool armed = s_prefs.getBool(KEY_ARMED, false);
    s_hour = s_prefs.getUChar(KEY_HOUR, SCHEDULE_DEFAULT_HOUR);
    s_minute = s_prefs.getUChar(KEY_MINUTE, SCHEDULE_DEFAULT_MINUTE);
    s_temp = s_prefs.getUShort(KEY_TEMP, settings_load_simple_target());
    s_dur_s = s_prefs.getUShort(KEY_DUR, SCHEDULE_DEFAULT_DUR_S);
    s_start_epoch = s_prefs.getULong(KEY_AT, 0);
    s_prefs.end();

    if (s_hour > 23) s_hour = SCHEDULE_DEFAULT_HOUR;
    if (s_minute > 59) s_minute = SCHEDULE_DEFAULT_MINUTE;
    if (s_temp < 50 || s_temp > 500) s_temp = 200;
    if (s_dur_s <= 0 || s_dur_s > SCHEDULE_MAX_DUR_S) s_dur_s = SCHEDULE_DEFAULT_DUR_S;

    s_state = (armed && s_start_epoch > 0) ? ScheduleState::ARMED : ScheduleState::OFF;
    Serial.printf("[AVVIO] %s %02d:%02d, %d\xC2\xB0" "C, timer %d:%02d\n",
                  s_state == ScheduleState::ARMED ? "Armato" : "Disattivato",
                  s_hour, s_minute, s_temp, s_dur_s / 60, s_dur_s % 60);
}

// ============================================
// Helpers
// ============================================

// Prossima occorrenza di hh:mm (oggi se non ancora passata, altrimenti domani)
static uint32_t next_start_epoch() {
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);
    t.tm_hour = s_hour;
    t.tm_min = s_minute;
    t.tm_sec = 0;
    t.tm_isdst = -1;  // mktime decide l'ora legale del giorno scelto
    time_t at = mktime(&t);
    if (at <= now) {
        t.tm_mday += 1;
        t.tm_hour = s_hour;
        t.tm_min = s_minute;
        t.tm_sec = 0;
        t.tm_isdst = -1;
        at = mktime(&t);
    }
    return (uint32_t)at;
}

static void set_note(const char* note) {
    strncpy(s_note, note, sizeof(s_note) - 1);
    s_note[sizeof(s_note) - 1] = '\0';
    Serial.printf("[AVVIO] %s\n", s_note);
}

// Fine dell'automatismo; con l'avvio scattato non resta nulla da salvare
static void finish(const char* note) {
    bool was_armed = (s_state == ScheduleState::ARMED);
    s_state = ScheduleState::OFF;
    set_note(note);
    if (was_armed) save();
}

static void power_off() {
    g_state.power_on = false;
    ssr_control_all_off();
    oven_state_go_standby();
}

// ============================================
// Avvio all'orario
// ============================================
static void check_start() {
    // L'orologio di riserva (senza NTP) puo' essere sbagliato di ore
    if (!wifi_time_is_synced()) return;

    time_t now = time(nullptr);
    if (now < (time_t)s_start_epoch) return;

    if (now - (time_t)s_start_epoch > SCHEDULE_LATE_MAX_S) {
        finish("Saltato: forno senza ora");
        return;
    }
    if (g_state.power_on) {
        // Chi l'ha acceso a mano lo sta usando: niente spegnimento automatico
        finish("Non partito: era gia' acceso");
        return;
    }
    if (g_state.error_fan || g_state.error_cielo || g_state.error_platea) {
        // ui_auto_power_on() azzererebbe l'errore ventola come il tasto
        // ACCENDI: senza nessuno davanti non si riparte da un arresto
        buzzer_error();
        finish("Non partito: errore");
        return;
    }

    ui_auto_set_target(s_temp);
    ui_auto_power_on();
    g_state.timer_active = false;
    g_state.timer_paused = false;
    g_state.timer_expired = false;
    g_state.timer_blink = false;
    g_state.timer_minutes = s_dur_s / 60;
    g_state.timer_seconds = s_dur_s % 60;

    s_heat_start_ms = millis();
    s_state = ScheduleState::HEATING;
    s_note[0] = '\0';
    save();  // non piu' armata: un riavvio non deve riaccenderlo
    buzzer_confirm();
    Serial.printf("[AVVIO] Acceso: %d\xC2\xB0" "C, timer %d:%02d a temperatura raggiunta\n",
                  s_temp, s_dur_s / 60, s_dur_s % 60);
}

// ============================================
// Update
// ============================================
void schedule_update() {
    switch (s_state) {
        case ScheduleState::OFF:
            return;

        case ScheduleState::ARMED: {
            unsigned long now = millis();
            if (now - s_last_check_ms < 1000) return;
            s_last_check_ms = now;
            check_start();
            return;
        }

        case ScheduleState::HEATING:
        case ScheduleState::TIMING:
            break;
    }

    // Spento a mano, da web o da un arresto di sicurezza
    if (!g_state.power_on) {
        finish("Annullato: forno spento");
        return;
    }

    // Senza sorveglianza una sonda guasta spegne tutto: col forno acceso a
    // mano resta solo l'allarme, qui nessuno lo sentirebbe
    if (g_state.error_cielo || g_state.error_platea) {
        power_off();
        buzzer_error();
        finish("Spento: errore sonda");
        return;
    }

    if (s_state == ScheduleState::HEATING) {
        if (g_state.timer_active) {
            // Timer avviato a mano prima della temperatura: vale quello
            s_state = ScheduleState::TIMING;
            return;
        }
        // In temperatura = entrambe le sonde al cutoff del preriscaldo, senza
        // aspettare la saturazione della pietra
        if (g_state.cielo_in_maintain && g_state.platea_in_maintain) {
            // Un timer cambiato a mano durante la salita resta com'e'
            if (g_state.timer_minutes == 0 && g_state.timer_seconds == 0) {
                g_state.timer_minutes = s_dur_s / 60;
                g_state.timer_seconds = s_dur_s % 60;
            }
            oven_state_timer_start();
            s_state = ScheduleState::TIMING;
            buzzer_confirm();
            Serial.printf("[AVVIO] In temperatura dopo %lu min: timer %d:%02d avviato\n",
                          (millis() - s_heat_start_ms) / 60000UL,
                          g_state.timer_minutes, g_state.timer_seconds);
            return;
        }
        if (millis() - s_heat_start_ms >= (unsigned long)SCHEDULE_HEAT_MAX_MIN * 60000UL) {
            power_off();
            buzzer_error();
            finish("Spento: temperatura non raggiunta");
        }
        return;
    }

    // TIMING
    if (g_state.timer_expired) {
        power_off();
        char note[40];
        snprintf(note, sizeof(note), "Spento a fine timer alle %02d:%02d",
                 g_state.hour, g_state.minute);
        finish(note);
    } else if (!g_state.timer_active) {
        // Timer azzerato a mano: decide chi e' davanti al forno
        finish("Annullato: timer fermato");
    }
}

// ============================================
// Comandi
// ============================================
bool schedule_arm() {
    if (schedule_is_running()) return false;
    if (!wifi_time_is_synced() || s_dur_s <= 0) return false;

    s_start_epoch = next_start_epoch();
    s_state = ScheduleState::ARMED;
    s_note[0] = '\0';
    save();
    Serial.printf("[AVVIO] Armato per le %02d:%02d (fra %d min)\n",
                  s_hour, s_minute, schedule_minutes_to_start());
    return true;
}

void schedule_cancel() {
    if (s_state == ScheduleState::OFF) return;
    bool running = schedule_is_running();
    s_state = ScheduleState::OFF;
    save();
    if (running) set_note("Annullato: forno lasciato acceso");
    else s_note[0] = '\0';
}

static void rearm_if_armed() {
    if (s_state == ScheduleState::ARMED && wifi_time_is_synced()) {
        s_start_epoch = next_start_epoch();
    }
    save();
}

void schedule_set_time(int hour, int minute) {
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) return;
    if (hour == s_hour && minute == s_minute) return;
    s_hour = hour;
    s_minute = minute;
    rearm_if_armed();
}

void schedule_set_temp(int temp) {
    temp = constrain(temp, 50, 500);
    if (temp == s_temp) return;
    s_temp = temp;
    save();
}

void schedule_set_duration_s(int seconds) {
    if (seconds <= 0 || seconds > SCHEDULE_MAX_DUR_S) return;
    if (seconds == s_dur_s) return;
    s_dur_s = seconds;
    save();
}

// ============================================
// Getters
// ============================================
ScheduleState schedule_get_state() { return s_state; }

bool schedule_is_running() {
    return s_state == ScheduleState::HEATING || s_state == ScheduleState::TIMING;
}

int schedule_get_hour() { return s_hour; }
int schedule_get_minute() { return s_minute; }
int schedule_get_temp() { return s_temp; }
int schedule_get_duration_s() { return s_dur_s; }

int schedule_minutes_to_start() {
    if (s_state != ScheduleState::ARMED || !wifi_time_is_synced()) return -1;
    long left = (long)s_start_epoch - (long)time(nullptr);
    if (left <= 0) return 0;
    return (int)((left + 59) / 60);
}

const char* schedule_get_note() { return s_note; }
