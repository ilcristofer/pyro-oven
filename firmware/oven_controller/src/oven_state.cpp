/**
 * Oven State Implementation - Effeuno style
 */

#include "oven_state.h"
#include "hardware_pins.h"
#include "wifi_time.h"
#include "buzzer.h"
#include "thermal_profile.h"
#include "ssr_control.h"
#include "schedule.h"

// ============================================
// Global state instance
// ============================================
OvenState g_state;

// ============================================
// Global programs array (inizializzato da programs_storage)
// ============================================
Program g_programs[MAX_PROGRAMS];

// Cached effective power (updated every 2s at sensor read, not 100x/sec)
static int s_cached_eff_power_cielo = 0;
static int s_cached_eff_power_platea = 0;

// Timing variables
static unsigned long last_second_tick = 0;
static unsigned long last_timer_tick = 0;
static unsigned long last_boost_tick = 0;
static unsigned long last_animation_tick = 0;
static unsigned long last_blink_tick = 0;

// Durata con cui e' partito il timer (secondi): alla scadenza si torna li'
static int s_timer_start_total_s = 0;

// Day names (Italian)
static const char* day_names[] = {"DOMENICA", "LUNEDI'", "MARTEDI'", "MERCOLEDI'", "GIOVEDI'", "VENERDI'", "SABATO"};

// Inactivity timeout (1 hour in milliseconds)
#define INACTIVITY_TIMEOUT 3600000

// ============================================
// Initialize state
// ============================================
void oven_state_init() {
    // Power state
    g_state.power_on = false;
    g_state.mode = OvenMode::STANDBY;
    g_state.previous_mode = OvenMode::STANDBY;

    // Temperatures
    g_state.temp_cielo = 25;
    g_state.temp_platea = 25;
    g_state.target_cielo = 155;
    g_state.target_platea = 155;

    // Power levels (default Effeuno style)
    g_state.power_cielo = 75;
    g_state.power_platea = 75;

    // Independent heating state (start in preheat)
    g_state.cielo_in_maintain = false;
    g_state.platea_in_maintain = false;

    // Boost
    g_state.boost_cielo = false;
    g_state.boost_platea = false;
    g_state.boost_time_cielo = 0;
    g_state.boost_time_platea = 0;
    g_state.boost_max_time = 60;  // 60 seconds default
    g_state.boost_power = 100;

    // Errors
    g_state.error_cielo = false;
    g_state.error_platea = false;
    g_state.error_msg_cielo[0] = '\0';
    g_state.error_msg_platea[0] = '\0';
    g_state.error_count_cielo = 0;
    g_state.error_count_platea = 0;

    // Pietra refrattaria e servizio pizze
    g_state.stone_ready = false;
    g_state.soak_start_ms = 0;
    g_state.service_mode = false;
    g_state.recovery_active = false;
    g_state.recovery_start_ms = 0;

    // Fan monitoring
    g_state.error_fan = false;
    g_state.error_msg_fan[0] = '\0';
    g_state.error_count_fan = 0;
    g_state.fan_rpm = 0;

    // Timer
    g_state.timer_minutes = 0;
    g_state.timer_seconds = 0;
    g_state.timer_active = false;
    g_state.timer_paused = false;
    g_state.timer_expired = false;
    g_state.timer_blink = false;

    // Clock (default time - overwritten by NTP if available)
    g_state.hour = 12;
    g_state.minute = 0;
    g_state.second = 0;
    g_state.day_of_week = 0;
    g_state.day = 1;
    g_state.month = 1;
    g_state.year = 2026;

    // Program (MANUAL mode, P0)
    g_state.program_number = 0;
    g_state.active_phase = 0;
    g_state.num_phases = 3;

    // Initialize phases (default)
    g_state.phases[0] = {155, 155, 2, 75, 75};  // Phase 1
    g_state.phases[1] = {156, 156, 2, 75, 75};  // Phase 2
    g_state.phases[2] = {157, 157, 2, 75, 75};  // Phase 3
    strncpy(g_state.program_name, "PYRO", MAX_PROGRAM_NAME - 1);
    g_state.program_name[MAX_PROGRAM_NAME - 1] = '\0';

    // Animation
    g_state.resistance_blink_on = true;

    // Inactivity
    g_state.last_activity_time = millis();
    g_state.auto_standby_enabled = true;

    // Init timing
    last_second_tick = millis();
    last_timer_tick = millis();
    last_boost_tick = millis();
    last_animation_tick = millis();
}

// ============================================
// Update oven mode
// ============================================
void oven_state_update_mode() {
    if (!g_state.power_on) {
        g_state.mode = OvenMode::STANDBY;
        g_state.boost_cielo = false;
        g_state.boost_platea = false;
        g_state.cielo_in_maintain = false;
        g_state.platea_in_maintain = false;
        g_state.service_mode = false;
        g_state.recovery_active = false;
        g_state.stone_ready = false;
        g_state.soak_start_ms = 0;
        return;
    }

    // Check for boost mode (takes priority)
    if (g_state.boost_cielo || g_state.boost_platea) {
        if (g_state.mode != OvenMode::BOOST) {
            g_state.previous_mode = g_state.mode;
        }
        g_state.mode = OvenMode::BOOST;
        return;
    }

    // Boost appena spento a mano: update_boost() ripristina il modo solo al
    // suo tick da 1s. Senza questo una cottura in corso veniva scambiata per
    // MAINTAIN/READY, usciva da COOKING e le fasi successive non partivano
    if (g_state.mode == OvenMode::BOOST) {
        g_state.mode = g_state.previous_mode;
    }

    // COOKING mode is sticky: exit only via standby
    if (g_state.mode == OvenMode::COOKING) {
        return;
    }

    // Check each resistance independently for PREHEAT -> MAINTAIN transition
    int delta_cielo = g_state.target_cielo - g_state.temp_cielo;
    int delta_platea = g_state.target_platea - g_state.temp_platea;

    int cutoff_c = thermal_profile_get_cutoff_cielo(g_state.target_cielo);
    int cutoff_p = thermal_profile_get_cutoff_platea(g_state.target_platea);

    // Target alzato a forno gia' a regime: si torna a salire. Senza questo
    // l'elemento restava a potenza di mantenimento e il forno si dichiarava
    // pronto anche con il target molto piu' in alto della temperatura.
    // Un target abbassato invece non richiede nulla: si scende da soli.
    static int s_prev_target_cielo = 0;
    static int s_prev_target_platea = 0;
    if (g_state.target_cielo != s_prev_target_cielo) {
        s_prev_target_cielo = g_state.target_cielo;
        if (g_state.cielo_in_maintain && delta_cielo > cutoff_c) {
            g_state.cielo_in_maintain = false;
            Serial.println("[STATE] Target cielo alzato -> torna in salita");
        }
    }
    if (g_state.target_platea != s_prev_target_platea) {
        s_prev_target_platea = g_state.target_platea;
        if (g_state.platea_in_maintain && delta_platea > cutoff_p) {
            g_state.platea_in_maintain = false;
            // La pietra dovra' risaturare al nuovo livello
            g_state.stone_ready = false;
            g_state.soak_start_ms = 0;
            Serial.println("[STATE] Target platea alzato -> torna in salita, pietra da risaturare");
        }
    }

    // Cielo: cutoff adattivo dal profilo termico
    if (!g_state.cielo_in_maintain && delta_cielo <= cutoff_c) {
        g_state.cielo_in_maintain = true;
        Serial.printf("[STATE] Cielo reached target (cutoff=%d) -> MAINTAIN power\n", cutoff_c);
    }

    // Platea: cutoff adattivo dal profilo termico
    if (!g_state.platea_in_maintain && delta_platea <= cutoff_p) {
        g_state.platea_in_maintain = true;
        Serial.printf("[STATE] Platea reached target (cutoff=%d) -> MAINTAIN power\n", cutoff_p);
    }

    // Update mode based on heating state
    bool cielo_ok = !g_state.error_cielo && g_state.cielo_in_maintain;
    bool platea_ok = !g_state.error_platea && g_state.platea_in_maintain;

    if (cielo_ok && platea_ok) {
        // Sonde a target. Ma la sonda platea sta SOTTO la pietra: la superficie
        // di cottura e' ancora piu' fredda finche' la pietra non e' satura.
        // READY = pietra carica; fino ad allora si resta in MAINTAIN.
        if (g_state.stone_ready) {
            if (g_state.mode != OvenMode::READY) {
                Serial.println("[STATE] Pietra carica -> READY");
            }
            g_state.mode = OvenMode::READY;
        } else {
            if (g_state.mode != OvenMode::MAINTAIN) {
                Serial.println("[STATE] Sonde a target -> MAINTAIN (saturazione pietra)");
            }
            g_state.mode = OvenMode::MAINTAIN;
        }
    } else if (g_state.mode == OvenMode::PREHEAT) {
        // At least one still heating, stay in PREHEAT
        // (mode stays PREHEAT)
    } else if (g_state.cielo_in_maintain || g_state.platea_in_maintain) {
        // One reached target, switch to MAINTAIN
        if (g_state.mode != OvenMode::MAINTAIN) {
            Serial.println("[STATE] Partial target reached -> MAINTAIN");
        }
        g_state.mode = OvenMode::MAINTAIN;
    }
}

// ============================================
// Get mode string
// ============================================
const char* oven_state_get_mode_string() {
    switch (g_state.mode) {
        case OvenMode::STANDBY:  return "STANDBY";
        case OvenMode::PREHEAT:  return "PREHEAT";
        case OvenMode::MAINTAIN: return "MAINTAIN";
        case OvenMode::READY:    return "READY";
        case OvenMode::COOKING:  return "COOKING";
        case OvenMode::BOOST:    return "BOOST";
        default: return "???";
    }
}

// ============================================
// Get day name
// ============================================
const char* oven_state_get_day_name(int day) {
    if (day >= 0 && day < 7) {
        return day_names[day];
    }
    return "???";
}

// ============================================
// Update clock
// ============================================
void oven_state_update_clock() {
    // Se l'ora NTP e' sincronizzata, usa quella
    if (wifi_time_is_synced()) {
        wifi_time_update_state();
        return;
    }

    // Fallback: incrementa manualmente (se NTP non disponibile)
    unsigned long now = millis();
    if (now - last_second_tick >= CLOCK_TICK_INTERVAL) {
        last_second_tick = now;

        g_state.second++;
        if (g_state.second >= 60) {
            g_state.second = 0;
            g_state.minute++;
            if (g_state.minute >= 60) {
                g_state.minute = 0;
                g_state.hour++;
                if (g_state.hour >= 24) {
                    g_state.hour = 0;
                    // Increment day (simplified, no month/year rollover)
                    g_state.day_of_week = (g_state.day_of_week + 1) % 7;
                    g_state.day++;
                }
            }
        }
    }
}

// ============================================
// Update timer countdown
// ============================================
void oven_state_update_timer() {
    unsigned long now = millis();

    // Timer countdown
    if (g_state.timer_active && !g_state.timer_paused) {
        if (now - last_timer_tick >= 1000) {
            last_timer_tick = now;

            if (g_state.timer_seconds > 0) {
                g_state.timer_seconds--;
            } else if (g_state.timer_minutes > 0) {
                g_state.timer_minutes--;
                g_state.timer_seconds = 59;
            } else {
                // Timer fase scaduto - passa alla fase successiva
                if (g_state.mode == OvenMode::COOKING && oven_state_has_next_phase()) {
                    // Allarme fine fase (suona ad ogni transizione)
                    buzzer_alert();
                    // Passa alla fase successiva
                    oven_state_next_phase();
                } else {
                    // Nessuna fase successiva o non in cottura
                    g_state.timer_active = false;
                    g_state.timer_expired = true;
                    last_blink_tick = now;
                    buzzer_alert();  // Allarme timer scaduto
                    // Pronto per la prossima infornata: si torna al valore con
                    // cui il timer era partito (anche 1:30), altrimenti alla
                    // durata della fase
                    int phase = g_state.active_phase;
                    if (s_timer_start_total_s > 0) {
                        g_state.timer_minutes = s_timer_start_total_s / 60;
                        g_state.timer_seconds = s_timer_start_total_s % 60;
                    } else if (phase < g_state.num_phases && g_state.phases[phase].duration > 0) {
                        g_state.timer_minutes = g_state.phases[phase].duration;
                        g_state.timer_seconds = 0;
                    }
                }
            }
        }
    }

    // Timer expired blink
    if (g_state.timer_expired) {
        if (now - last_blink_tick >= 500) {
            last_blink_tick = now;
            g_state.timer_blink = !g_state.timer_blink;
        }
    }
}

// ============================================
// Update boost countdown
// ============================================
void oven_state_update_boost() {
    unsigned long now = millis();

    if (now - last_boost_tick >= 1000) {
        last_boost_tick = now;

        // Countdown boost cielo
        if (g_state.boost_cielo && g_state.boost_time_cielo > 0) {
            g_state.boost_time_cielo--;
            if (g_state.boost_time_cielo <= 0) {
                g_state.boost_cielo = false;
            }
        }

        // Countdown boost platea
        if (g_state.boost_platea && g_state.boost_time_platea > 0) {
            g_state.boost_time_platea--;
            if (g_state.boost_time_platea <= 0) {
                g_state.boost_platea = false;
            }
        }

        // Return to previous mode if both boosts are off
        if (!g_state.boost_cielo && !g_state.boost_platea && g_state.mode == OvenMode::BOOST) {
            g_state.mode = g_state.previous_mode;
        }
    }
}

// ============================================
// Update resistance animation
// ============================================
void oven_state_update_animation() {
    bool should_blink = g_state.power_on &&
                       (g_state.mode == OvenMode::PREHEAT ||
                        g_state.mode == OvenMode::BOOST ||
                        oven_state_resistance_cielo_on() ||
                        oven_state_resistance_platea_on());

    if (should_blink) {
        unsigned long now = millis();
        if (now - last_animation_tick >= ANIMATION_INTERVAL) {
            last_animation_tick = now;
            g_state.resistance_blink_on = !g_state.resistance_blink_on;
        }
    } else {
        g_state.resistance_blink_on = true;
    }
}

// ============================================
// Check if resistances should be on
// ============================================
bool oven_state_resistance_cielo_on() {
    if (!g_state.power_on || g_state.error_cielo || g_state.error_fan || g_state.mode == OvenMode::STANDBY) {
        return false;
    }

    // Tetto assoluto, vale anche per il boost
    if (g_state.temp_cielo >= TEMP_ABS_MAX) return false;

    // Il boost scalda oltre il termostato, fino a un margine sopra il target
    if (g_state.boost_cielo) return g_state.temp_cielo < g_state.target_cielo + BOOST_MAX_OVER;

    // Normal heating logic
    return (g_state.temp_cielo < g_state.target_cielo);
}

bool oven_state_resistance_platea_on() {
    if (!g_state.power_on || g_state.error_platea || g_state.error_fan || g_state.mode == OvenMode::STANDBY) {
        return false;
    }

    if (g_state.temp_platea >= TEMP_ABS_MAX) return false;

    if (g_state.boost_platea) return g_state.temp_platea < g_state.target_platea + BOOST_MAX_OVER;

    // Normal heating logic
    return (g_state.temp_platea < g_state.target_platea);
}

// ============================================
// Mode control functions
// ============================================
void oven_state_start_preheat() {
    if (!g_state.power_on) return;

    // Can only preheat if at least one resistance needs heating
    int delta_cielo = g_state.target_cielo - g_state.temp_cielo;
    int delta_platea = g_state.target_platea - g_state.temp_platea;
    if (delta_cielo < 6 && delta_platea < 6) {
        // Forno gia' a temperatura: niente salita, si va subito a regime
        // (prima si restava in STANDBY fino al tick sensori successivo)
        oven_state_start_maintain();
        return;
    }

    g_state.mode = OvenMode::PREHEAT;

    // Reset individual maintain states
    g_state.cielo_in_maintain = (delta_cielo < 6);  // Already at target
    g_state.platea_in_maintain = (delta_platea < 6);  // Already at target

    // La potenza di preriscaldamento la decide il profilo termico
    // (oven_state_update_effective_power): power_cielo/platea restano quelle
    // di cottura. Sovrascriverle qui le faceva finire salvate in P0

    Serial.printf("[STATE] PREHEAT started - Cielo: %s, Platea: %s\n",
                  g_state.cielo_in_maintain ? "MAINTAIN" : "PREHEAT",
                  g_state.platea_in_maintain ? "MAINTAIN" : "PREHEAT");

    oven_state_update_effective_power();
    oven_state_register_activity();
}

void oven_state_start_maintain() {
    if (!g_state.power_on) return;

    g_state.mode = OvenMode::MAINTAIN;
    oven_state_update_effective_power();
    oven_state_register_activity();
}

void oven_state_start_cooking() {
    if (!g_state.power_on) return;

    // Target e potenze della fase attiva sono gia' in g_state (caricati con
    // la fase) e l'utente puo' averli ritoccati dal tastierino: valgono quelli.
    // Ricaricarli dalla fase annullava le modifiche appena fatte
    int phase = g_state.active_phase;
    if (phase < g_state.num_phases) {
        g_state.phases[phase].temp_cielo = g_state.target_cielo;
        g_state.phases[phase].temp_platea = g_state.target_platea;
        g_state.phases[phase].power_cielo = g_state.power_cielo;
        g_state.phases[phase].power_platea = g_state.power_platea;
    }

    // Se il timer non è impostato, carica dalla fase attiva
    if (g_state.timer_minutes == 0 && g_state.timer_seconds == 0) {
        if (phase < g_state.num_phases && g_state.phases[phase].duration > 0) {
            g_state.timer_minutes = g_state.phases[phase].duration;
            g_state.timer_seconds = 0;
        }
    }

    // Entra in COOKING (con o senza timer)
    g_state.timer_expired = false;
    g_state.mode = OvenMode::COOKING;

    if (g_state.timer_minutes > 0 || g_state.timer_seconds > 0) {
        oven_state_timer_start();
        Serial.printf("Cottura avviata - Fase %d: %d min\n",
                      g_state.active_phase + 1, g_state.timer_minutes);
    } else {
        g_state.timer_active = false;
        g_state.timer_paused = false;
        Serial.printf("Cottura avviata - Fase %d: senza timer\n",
                      g_state.active_phase + 1);
    }
    oven_state_register_activity();
}

void oven_state_toggle_boost_cielo() {
    if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) return;

    g_state.boost_cielo = !g_state.boost_cielo;
    if (g_state.boost_cielo) {
        g_state.boost_time_cielo = g_state.boost_max_time;
    } else {
        g_state.boost_time_cielo = 0;
    }
    oven_state_update_effective_power();
    oven_state_register_activity();
}

void oven_state_toggle_boost_platea() {
    if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) return;

    g_state.boost_platea = !g_state.boost_platea;
    if (g_state.boost_platea) {
        g_state.boost_time_platea = g_state.boost_max_time;
    } else {
        g_state.boost_time_platea = 0;
    }
    oven_state_update_effective_power();
    oven_state_register_activity();
}

void oven_state_go_standby() {
    g_state.mode = OvenMode::STANDBY;
    g_state.cielo_in_maintain = false;
    g_state.platea_in_maintain = false;
    g_state.timer_active = false;
    g_state.timer_paused = false;
    g_state.boost_cielo = false;
    g_state.boost_platea = false;
    g_state.service_mode = false;
    g_state.recovery_active = false;
    g_state.stone_ready = false;
    g_state.soak_start_ms = 0;
    oven_state_update_effective_power();
    thermal_profile_save_if_dirty();
    oven_state_register_activity();
}

// ============================================
// Phase Management
// ============================================

// Carica i parametri della fase specificata
void oven_state_load_phase(int phase_index) {
    if (phase_index < 0 || phase_index >= g_state.num_phases) {
        return;
    }

    Phase& phase = g_state.phases[phase_index];

    // Carica temperature target
    g_state.target_cielo = phase.temp_cielo;
    g_state.target_platea = phase.temp_platea;

    // Carica potenze
    g_state.power_cielo = phase.power_cielo;
    g_state.power_platea = phase.power_platea;

    // Carica timer (duration è in minuti)
    g_state.timer_minutes = phase.duration;
    g_state.timer_seconds = 0;

    // Aggiorna fase attiva
    g_state.active_phase = phase_index;

    // Reset stato timer
    g_state.timer_expired = false;
    g_state.timer_blink = false;

    Serial.printf("Fase %d caricata: %d°C/%d°C, %d:%02d, %d%%/%d%%\n",
                  phase_index + 1,
                  phase.temp_cielo, phase.temp_platea,
                  g_state.timer_minutes, g_state.timer_seconds,
                  phase.power_cielo, phase.power_platea);

    oven_state_register_activity();
}

// Controlla se esiste una fase successiva
bool oven_state_has_next_phase() {
    return (g_state.active_phase + 1) < g_state.num_phases;
}

// Passa alla fase successiva
void oven_state_next_phase() {
    if (oven_state_has_next_phase()) {
        int next = g_state.active_phase + 1;
        oven_state_load_phase(next);

        // Se in cottura, riavvia il timer automaticamente
        if (g_state.mode == OvenMode::COOKING) {
            oven_state_timer_start();
            Serial.printf("Fase %d avviata automaticamente\n", next + 1);
        }
    } else {
        // Nessuna fase successiva, cottura completata
        g_state.timer_active = false;
        g_state.timer_expired = true;
        g_state.mode = OvenMode::READY;
        buzzer_alert();  // Allarme cottura completata
        Serial.println("Tutte le fasi completate!");
    }
}

// ============================================
// Effective power cache (avoids expensive thermal_profile calc at 100Hz SSR rate)
// ============================================
void oven_state_update_effective_power() {
    // In PREHEAT, compute preheat power ONCE for both elements
    int preheat_c = 100, preheat_p = 100;
    if (g_state.mode == OvenMode::PREHEAT) {
        thermal_profile_get_preheat_power(
            g_state.temp_cielo, g_state.target_cielo,
            g_state.temp_platea, g_state.target_platea,
            &preheat_c, &preheat_p);
    }

    // Cielo
    if (g_state.boost_cielo) {
        s_cached_eff_power_cielo = g_state.boost_power;
    } else {
        switch (g_state.mode) {
            case OvenMode::PREHEAT:
                s_cached_eff_power_cielo = g_state.cielo_in_maintain
                    ? (int)thermal_profile_get_maintain_power_cielo(g_state.target_cielo)
                    : preheat_c;
                break;
            case OvenMode::MAINTAIN:
            case OvenMode::READY:
                s_cached_eff_power_cielo =
                    (g_state.recovery_active ||
                     g_state.temp_cielo < g_state.target_cielo - RECOVERY_THRESHOLD)
                    ? 100
                    : (int)thermal_profile_get_maintain_power_cielo(g_state.target_cielo);
                break;
            default:
                // In cottura il recupero dopo un'infornata vale come altrove
                s_cached_eff_power_cielo = g_state.recovery_active ? 100 : g_state.power_cielo;
                break;
        }
    }

    // Platea
    if (g_state.boost_platea) {
        s_cached_eff_power_platea = g_state.boost_power;
    } else {
        switch (g_state.mode) {
            case OvenMode::PREHEAT:
                s_cached_eff_power_platea = g_state.platea_in_maintain
                    ? (int)thermal_profile_get_maintain_power_platea(g_state.target_platea)
                    : preheat_p;
                break;
            case OvenMode::MAINTAIN:
            case OvenMode::READY:
                s_cached_eff_power_platea =
                    (g_state.recovery_active ||
                     g_state.temp_platea < g_state.target_platea - RECOVERY_THRESHOLD)
                    ? 100
                    : (int)thermal_profile_get_maintain_power_platea(g_state.target_platea);
                break;
            default:
                s_cached_eff_power_platea = g_state.recovery_active ? 100 : g_state.power_platea;
                break;
        }
    }
}

// ============================================
// Scostamento reale dal target
// A 380C il forno recupera ~3.7 C/min: dopo un'infornata puo' restare decine
// di gradi sotto il setpoint per minuti, e dirsi "pronto" sarebbe falso.
// ============================================
bool oven_state_temp_deficit(int* deficit_out, int* eta_out) {
    if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) return false;

    int dc = g_state.error_cielo ? 0 : (g_state.target_cielo - g_state.temp_cielo);
    int dp = g_state.error_platea ? 0 : (g_state.target_platea - g_state.temp_platea);
    int worst = (dc > dp) ? dc : dp;

    if (worst < TEMP_DEFICIT_WARN) return false;

    if (deficit_out) *deficit_out = worst;
    if (eta_out) {
        int ec = thermal_profile_get_target_eta_minutes(true);
        int ep = thermal_profile_get_target_eta_minutes(false);
        *eta_out = (ec > ep) ? ec : ep;
    }
    return true;
}

// ============================================
// Servizio pizze: infornate in sequenza
// ============================================
bool oven_state_service_available() {
    // Solo a forno in temperatura: spento o in preriscaldamento non c'e'
    // nulla da recuperare (e a forno spento lo stato verrebbe azzerato)
    if (!g_state.power_on) return false;
    return g_state.mode == OvenMode::MAINTAIN || g_state.mode == OvenMode::READY ||
           g_state.mode == OvenMode::COOKING || g_state.mode == OvenMode::BOOST;
}

bool oven_state_set_service_mode(bool on) {
    if (g_state.service_mode == on) return true;
    if (on && !oven_state_service_available()) {
        Serial.println("[SERVIZIO] non attivabile: forno non in temperatura");
        return false;
    }

    g_state.service_mode = on;
    if (!on) {
        g_state.recovery_active = false;
    } else {
        // Le infornate falsano le misure in corso (soak e bias ETA)
        thermal_profile_invalidate_session();
    }
    Serial.printf("[SERVIZIO] %s\n", on ? "attivo" : "disattivato");
    return true;
}

// ============================================
// Rilevamento infornata e recupero a piena potenza
// La sonda CIELO e' in camera: vede l'apertura della porta e la pizza fredda
// in pochi secondi, mentre la sonda sotto la pietra se ne accorge minuti dopo.
// ============================================
void oven_state_update_recovery() {
    if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) {
        g_state.recovery_active = false;
        return;
    }

    // In PREHEAT e BOOST si sta gia' spingendo al massimo: nulla da recuperare,
    // e la fine di un boost sarebbe scambiata per un'infornata
    if (g_state.mode == OvenMode::PREHEAT || g_state.mode == OvenMode::BOOST) {
        g_state.recovery_active = false;
        return;
    }

    unsigned long now = millis();

    // --- Recupero in corso: finisce a temperatura recuperata o per tetto ---
    if (g_state.recovery_active) {
        bool recovered = (g_state.temp_cielo >= g_state.target_cielo) &&
                         (g_state.temp_platea >= g_state.target_platea);
        bool timeout = (now - g_state.recovery_start_ms) >=
                       (unsigned long)RECOVERY_MAX_SECONDS * 1000UL;

        if (recovered || timeout) {
            g_state.recovery_active = false;
            Serial.printf("[RECUPERO] Fine dopo %lus (%s)\n",
                           (now - g_state.recovery_start_ms) / 1000UL,
                           timeout ? "tetto durata" : "temperatura recuperata");
        }
        return;
    }

    // --- Rilevamento: solo durante il servizio pizze ---
    if (!g_state.service_mode) return;

    // Il servizio si disattiva da solo se ci si dimentica di spegnerlo
    if (now - g_state.last_activity_time >= (unsigned long)SERVICE_AUTO_OFF_MIN * 60000UL) {
        oven_state_set_service_mode(false);
        return;
    }

    float drop = thermal_profile_get_drop_cielo_20s();
    if (drop >= (float)INFORNATA_DROP_C) {
        g_state.recovery_active = true;
        g_state.recovery_start_ms = now;
        // Chi inforna sta usando il forno anche senza toccare lo schermo:
        // senza questo il forno si spegneva per inattivita' a meta' serata
        g_state.last_activity_time = now;
        thermal_profile_invalidate_session();
        Serial.printf("[RECUPERO] Infornata rilevata: cielo -%.0f C in 20s\n", drop);
    }
}

int oven_state_get_effective_power_cielo() {
    return s_cached_eff_power_cielo;
}

int oven_state_get_effective_power_platea() {
    return s_cached_eff_power_platea;
}

// ============================================
// Activity tracking
// ============================================
void oven_state_register_activity() {
    g_state.last_activity_time = millis();

    // Qualsiasi tocco dell'utente cancella il lampeggio "TEMPO SCADUTO"
    if (g_state.timer_expired) {
        g_state.timer_expired = false;
        g_state.timer_blink = false;
    }
}

void oven_state_timer_start() {
    g_state.timer_active = true;
    g_state.timer_paused = false;
    g_state.timer_expired = false;
    g_state.timer_blink = false;
    s_timer_start_total_s = g_state.timer_minutes * 60 + g_state.timer_seconds;
    last_timer_tick = millis();  // il primo secondo dura un secondo
}

int oven_state_inactivity_remaining_s() {
    if (!g_state.auto_standby_enabled || !g_state.power_on) return -1;
    if (g_state.mode == OvenMode::STANDBY) return -1;
    if (g_state.timer_active) return -1;
    // Avvio programmato: la salita puo' durare piu' di un'ora senza nessuno
    // davanti, e lo spegnimento lo decide il suo timer
    if (schedule_is_running()) return -1;

    unsigned long idle = millis() - g_state.last_activity_time;
    if (idle >= INACTIVITY_TIMEOUT) return 0;
    return (int)((INACTIVITY_TIMEOUT - idle) / 1000UL);
}

void oven_state_check_inactivity() {
    if (!g_state.auto_standby_enabled || !g_state.power_on) return;
    if (g_state.mode == OvenMode::STANDBY) return;
    if (g_state.timer_active) return;  // Don't auto-standby during cooking
    if (schedule_is_running()) return;

    unsigned long now = millis();
    if (now - g_state.last_activity_time >= INACTIVITY_TIMEOUT) {
        // Va spento davvero: con power_on ancora vero update_mode() rimetteva
        // il forno in MAINTAIN entro 2s e le resistenze ripartivano
        g_state.power_on = false;
        ssr_control_all_off();
        oven_state_go_standby();
        buzzer_alert();
        Serial.println("Auto-standby due to inactivity");
    }
}
