/**
 * Oven State Management
 * Centralized state for the pizza oven - Effeuno style
 */

#ifndef OVEN_STATE_H
#define OVEN_STATE_H

#include <Arduino.h>

// ============================================
// Phase/Program Definition
// ============================================
#define MAX_PHASES        3
#define MAX_PROGRAMS      20    // P0-P19
#define MAX_PROGRAM_NAME  17    // 16 char + null terminator
#define PROGRAM_MANUAL    0     // P0 = Manuale

struct Phase {
    int temp_cielo;     // Top temperature target (°C)
    int temp_platea;    // Bottom temperature target (°C)
    int duration;       // Duration in minutes (0 = infinite)
    int power_cielo;    // Power percentage cielo (0-100%)
    int power_platea;   // Power percentage platea (0-100%)
};

// ============================================
// Program Definition (31 programmi salvati in NVS)
// ============================================
struct Program {
    char name[MAX_PROGRAM_NAME];    // Nome programma (max 16 char)
    uint8_t num_phases;             // Numero fasi (1-3)
    Phase phases[MAX_PHASES];       // 3 fasi
    uint8_t pot_preheat_cielo;      // Potenza preriscaldamento cielo (0-100%)
    uint8_t pot_preheat_platea;     // Potenza preriscaldamento platea (0-100%)
    uint8_t pot_maintain_cielo;     // Potenza mantenimento cielo (0-100%)
    uint8_t pot_maintain_platea;    // Potenza mantenimento platea (0-100%)
    uint8_t boost_time;             // Tempo boost (0-180 sec)
};

// ============================================
// Oven Operating Mode (Effeuno style)
// ============================================
enum class OvenMode {
    STANDBY,    // Forno in attesa di comandi
    PREHEAT,    // Preriscaldamento
    MAINTAIN,   // Mantenimento temperatura
    READY,      // Pronto per cottura
    COOKING,    // In cottura (timer attivo)
    BOOST       // Modalità boost attiva
};

// ============================================
// Oven State Structure
// ============================================
struct OvenState {
    // Power state
    bool power_on;
    OvenMode mode;
    OvenMode previous_mode;  // Per tornare allo stato precedente dopo boost

    // Current temperatures (from sensors)
    int temp_cielo;
    int temp_platea;
    int temp_ambient;   // Cold junction temp (media dei 2 MAX31855)

    // Target temperatures (set-point)
    int target_cielo;
    int target_platea;

    // Power levels (0-100%)
    int power_cielo;
    int power_platea;

    // Independent heating state per resistance
    bool cielo_in_maintain;     // true when cielo reached target
    bool platea_in_maintain;    // true when platea reached target

    // Boost mode
    bool boost_cielo;
    bool boost_platea;
    int boost_time_cielo;      // Tempo rimanente boost cielo (sec)
    int boost_time_platea;     // Tempo rimanente boost platea (sec)
    int boost_max_time;        // Tempo massimo boost (sec, max 180)
    int boost_power;           // Percentuale boost (fino a 100%)

    // Sensor errors
    bool error_cielo;
    bool error_platea;
    char error_msg_cielo[16];
    char error_msg_platea[16];
    int error_count_cielo;
    int error_count_platea;

    // Fan monitoring
    bool error_fan;
    char error_msg_fan[16];
    int error_count_fan;
    uint16_t fan_rpm;

    // Timer countdown
    int timer_minutes;
    int timer_seconds;
    bool timer_active;
    bool timer_paused;
    bool timer_expired;
    bool timer_blink;

    // Clock
    int hour;
    int minute;
    int second;
    int day_of_week;  // 0=DOM, 1=LUN, 2=MAR, 3=MER, 4=GIO, 5=VEN, 6=SAB
    int day;
    int month;
    int year;

    // Program/Phases
    int program_number;       // 0 = MANUAL, 1-30 = programmi
    int active_phase;
    int num_phases;
    Phase phases[MAX_PHASES];
    char program_name[MAX_PROGRAM_NAME];

    // Pietra refrattaria: la sonda platea sta SOTTO la pietra, quindi arriva
    // a target prima che la superficie di cottura sia satura di calore.
    // stone_ready = pietra carica, forno davvero pronto per infornare.
    bool stone_ready;
    unsigned long soak_start_ms;     // inizio saturazione (0 = non iniziata)

    // Servizio pizze: infornate in sequenza, recupero automatico
    bool service_mode;               // congela anche l'auto-learning
    bool recovery_active;            // recupero a piena potenza in corso
    unsigned long recovery_start_ms;

    // Animation state
    bool resistance_blink_on;

    // Inactivity timeout (auto-standby after 1 hour)
    unsigned long last_activity_time;
    bool auto_standby_enabled;
};

// ============================================
// Global state instance
// ============================================
extern OvenState g_state;

// ============================================
// Global programs array (31 programmi)
// ============================================
extern Program g_programs[MAX_PROGRAMS];

// ============================================
// State management functions
// ============================================

// Initialize state with defaults
void oven_state_init();

// Update oven mode based on temperatures and conditions
void oven_state_update_mode();

// Get mode as string for display
const char* oven_state_get_mode_string();

// Day of week names (Italian)
const char* oven_state_get_day_name(int day);

// Update clock (call every loop iteration)
void oven_state_update_clock();

// Update timer countdown (call every loop iteration)
void oven_state_update_timer();

// Update boost countdown (call every loop iteration)
void oven_state_update_boost();

// Update resistance animation (call every loop iteration)
void oven_state_update_animation();

// Check if cielo resistance should be on
bool oven_state_resistance_cielo_on();

// Check if platea resistance should be on
bool oven_state_resistance_platea_on();

// Mode control functions
void oven_state_start_preheat();
void oven_state_start_maintain();
void oven_state_start_cooking();
void oven_state_toggle_boost_cielo();
void oven_state_toggle_boost_platea();
void oven_state_go_standby();

// Phase management
void oven_state_load_phase(int phase_index);  // Carica parametri fase nel timer e target
void oven_state_next_phase();                  // Passa alla fase successiva
bool oven_state_has_next_phase();              // Controlla se c'è una fase successiva

// Update cached effective power (call after sensor read and state changes)
void oven_state_update_effective_power();

// Servizio pizze (infornate in sequenza): si attiva solo a forno in
// temperatura (mantenimento, pronto, cottura, boost). False = rifiutato
bool oven_state_service_available();
bool oven_state_set_service_mode(bool on);

// True se un elemento e' sotto il proprio target di piu' di TEMP_DEFICIT_WARN:
// il forno non e' realmente in temperatura, anche se ha gia' preriscaldato.
// Restituisce i gradi mancanti e i minuti stimati per recuperarli.
bool oven_state_temp_deficit(int* deficit_out, int* eta_out);

// Rileva l'infornata dal crollo della sonda cielo e gestisce il recupero
// a piena potenza (chiamare a ogni lettura sensori)
void oven_state_update_recovery();

// Get effective power (returns cached value)
int oven_state_get_effective_power_cielo();
int oven_state_get_effective_power_platea();

// Timer: avvia il conto alla rovescia dal valore attuale e lo ricorda, cosi'
// alla scadenza si ritrova pronto per la pizza successiva
void oven_state_timer_start();

// Activity tracking
void oven_state_register_activity();
void oven_state_check_inactivity();

// Preavviso prima dello spegnimento automatico per inattivita'
#define INACTIVITY_WARN_S 300
// Secondi allo spegnimento automatico, -1 se non e' in programma
// (forno spento, timer in corso, auto-standby disattivato)
int oven_state_inactivity_remaining_s();

#endif // OVEN_STATE_H
