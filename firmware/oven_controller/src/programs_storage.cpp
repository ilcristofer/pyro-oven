/**
 * Programs Storage Implementation
 * Usa ESP32 Preferences (NVS) per persistenza programmi
 */

#include "programs_storage.h"
#include "settings_storage.h"
#include <Preferences.h>

// Namespace NVS per programmi
#define PREFS_PROGRAMS_NS "progs"

// Chiavi NVS
#define KEY_CURRENT   "cur"
#define KEY_INIT      "init"
#define KEY_VERSION   "ver"
#define PROGRAMS_VERSION 3  // v3.23: potenze ricalibrate per Vevor 1000W+1000W simmetrico

// Preferences instance
static Preferences s_prefs;

// Programma corrente selezionato
static uint8_t s_current_program = PROGRAM_MANUAL;

// ============================================
// Programmi Default di Fabbrica
// ============================================
static void set_default_program(uint8_t index) {
    Program* p = &g_programs[index];

    // Reset base
    memset(p, 0, sizeof(Program));
    p->num_phases = 1;
    p->pot_preheat_cielo = 90;
    p->pot_preheat_platea = 80;
    p->pot_maintain_cielo = 65;
    p->pot_maintain_platea = 40;
    p->boost_time = 60;

    // Valori default per fase 1
    p->phases[0].temp_cielo = 155;
    p->phases[0].temp_platea = 155;
    p->phases[0].duration = 2;  // 2 minuti
    p->phases[0].power_cielo = 75;
    p->phases[0].power_platea = 75;

    // Nome e parametri specifici per indice
    switch (index) {
        case 0:  // MANUALE - preheat 100%, maintain 95% (fissi)
            strncpy(p->name, "MANUALE", MAX_PROGRAM_NAME - 1);
            p->pot_preheat_cielo = 100;
            p->pot_preheat_platea = 100;
            p->pot_maintain_cielo = 95;
            p->pot_maintain_platea = 95;
            break;

        case 1:  // NAPOLETANA - cottura rapida ~90s, pietra biscotto
            strncpy(p->name, "NAPOLETANA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 450;
            p->phases[0].temp_platea = 420;
            p->phases[0].duration = 2;
            p->phases[0].power_cielo = 100;  // max cielo per leopardatura (1000W)
            p->phases[0].power_platea = 85;
            p->pot_preheat_cielo = 100;
            p->pot_preheat_platea = 100;
            p->pot_maintain_cielo = 90;
            p->pot_maintain_platea = 85;
            p->boost_time = 30;
            break;

        case 2:  // ROMANA - sottile e croccante, 3-4 min
            strncpy(p->name, "ROMANA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 320;
            p->phases[0].temp_platea = 310;
            p->phases[0].duration = 4;
            p->phases[0].power_cielo = 100;
            p->phases[0].power_platea = 80;
            p->pot_preheat_cielo = 95;
            p->pot_preheat_platea = 90;
            p->pot_maintain_cielo = 70;
            p->pot_maintain_platea = 65;
            p->boost_time = 45;
            break;

        case 3:  // TEGLIA - piu' platea per fondo croccante attraverso teglia
            strncpy(p->name, "TEGLIA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 280;
            p->phases[0].temp_platea = 290;
            p->phases[0].duration = 8;
            p->phases[0].power_cielo = 75;
            p->phases[0].power_platea = 90;
            p->pot_preheat_cielo = 90;
            p->pot_preheat_platea = 85;
            p->pot_maintain_cielo = 65;
            p->pot_maintain_platea = 60;
            p->boost_time = 60;
            break;

        case 4:  // AMERICANA - impasto spesso, testato su Vevor 1000W
            strncpy(p->name, "AMERICANA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 365;
            p->phases[0].temp_platea = 330;
            p->phases[0].duration = 5;
            p->phases[0].power_cielo = 100;
            p->phases[0].power_platea = 80;
            p->pot_preheat_cielo = 100;
            p->pot_preheat_platea = 100;
            p->pot_maintain_cielo = 60;
            p->pot_maintain_platea = 55;
            p->boost_time = 120;
            break;

        case 5:  // FOCACCIA - piu' platea per fondo dorato
            strncpy(p->name, "FOCACCIA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 230;
            p->phases[0].temp_platea = 250;
            p->phases[0].duration = 15;
            p->phases[0].power_cielo = 70;
            p->phases[0].power_platea = 90;
            p->pot_preheat_cielo = 85;
            p->pot_preheat_platea = 80;
            p->pot_maintain_cielo = 55;
            p->pot_maintain_platea = 50;
            p->boost_time = 60;
            break;

        case 6:  // PALA - pizza alla pala, media temp
            strncpy(p->name, "PALA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 310;
            p->phases[0].temp_platea = 300;
            p->phases[0].duration = 5;
            p->phases[0].power_cielo = 100;
            p->phases[0].power_platea = 80;
            p->pot_preheat_cielo = 95;
            p->pot_preheat_platea = 90;
            p->pot_maintain_cielo = 70;
            p->pot_maintain_platea = 65;
            p->boost_time = 45;
            break;

        case 7:  // PINSA - idratazione alta, cottura media
            strncpy(p->name, "PINSA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 300;
            p->phases[0].temp_platea = 290;
            p->phases[0].duration = 5;
            p->phases[0].power_cielo = 100;
            p->phases[0].power_platea = 80;
            p->pot_preheat_cielo = 90;
            p->pot_preheat_platea = 85;
            p->pot_maintain_cielo = 65;
            p->pot_maintain_platea = 60;
            p->boost_time = 45;
            break;

        case 8:  // PANE - 2 fasi: crosta poi cottura interna
            strncpy(p->name, "PANE", MAX_PROGRAM_NAME - 1);
            p->num_phases = 2;
            p->phases[0].temp_cielo = 250;
            p->phases[0].temp_platea = 240;
            p->phases[0].duration = 10;
            p->phases[0].power_cielo = 80;
            p->phases[0].power_platea = 70;
            p->phases[1].temp_cielo = 220;
            p->phases[1].temp_platea = 210;
            p->phases[1].duration = 20;
            p->phases[1].power_cielo = 65;
            p->phases[1].power_platea = 65;
            p->pot_preheat_cielo = 95;
            p->pot_preheat_platea = 90;
            p->pot_maintain_cielo = 60;
            p->pot_maintain_platea = 55;
            p->boost_time = 0;
            break;

        case 9:  // ARROSTO - 2 fasi: rosolatura poi cottura lenta
            strncpy(p->name, "ARROSTO", MAX_PROGRAM_NAME - 1);
            p->num_phases = 2;
            p->phases[0].temp_cielo = 220;
            p->phases[0].temp_platea = 200;
            p->phases[0].duration = 15;
            p->phases[0].power_cielo = 85;
            p->phases[0].power_platea = 55;
            p->phases[1].temp_cielo = 180;
            p->phases[1].temp_platea = 170;
            p->phases[1].duration = 30;
            p->phases[1].power_cielo = 55;
            p->phases[1].power_platea = 50;
            p->pot_preheat_cielo = 85;
            p->pot_preheat_platea = 75;
            p->pot_maintain_cielo = 55;
            p->pot_maintain_platea = 45;
            p->boost_time = 0;
            break;

        case 10: // POLLO - 2 fasi: doratura poi cottura
            strncpy(p->name, "POLLO", MAX_PROGRAM_NAME - 1);
            p->num_phases = 2;
            p->phases[0].temp_cielo = 210;
            p->phases[0].temp_platea = 200;
            p->phases[0].duration = 15;
            p->phases[0].power_cielo = 80;
            p->phases[0].power_platea = 55;
            p->phases[1].temp_cielo = 190;
            p->phases[1].temp_platea = 180;
            p->phases[1].duration = 25;
            p->phases[1].power_cielo = 60;
            p->phases[1].power_platea = 50;
            p->pot_preheat_cielo = 85;
            p->pot_preheat_platea = 75;
            p->pot_maintain_cielo = 55;
            p->pot_maintain_platea = 45;
            p->boost_time = 0;
            break;

        case 11: // PATATE - calore alto per crosta croccante
            strncpy(p->name, "PATATE", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 210;
            p->phases[0].temp_platea = 200;
            p->phases[0].duration = 30;
            p->phases[0].power_cielo = 75;
            p->phases[0].power_platea = 60;
            p->pot_preheat_cielo = 85;
            p->pot_preheat_platea = 80;
            p->pot_maintain_cielo = 55;
            p->pot_maintain_platea = 50;
            p->boost_time = 0;
            break;

        case 12: // TORTA - cottura dolce e uniforme, platea dominante
            strncpy(p->name, "TORTA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 170;
            p->phases[0].temp_platea = 160;
            p->phases[0].duration = 35;
            p->phases[0].power_cielo = 45;
            p->phases[0].power_platea = 50;
            p->pot_preheat_cielo = 80;
            p->pot_preheat_platea = 70;
            p->pot_maintain_cielo = 45;
            p->pot_maintain_platea = 35;
            p->boost_time = 0;
            break;

        case 13: // TORTA SALATA - base croccante, platea dominante
            strncpy(p->name, "TORTA SALATA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 190;
            p->phases[0].temp_platea = 185;
            p->phases[0].duration = 30;
            p->phases[0].power_cielo = 50;
            p->phases[0].power_platea = 65;
            p->pot_preheat_cielo = 85;
            p->pot_preheat_platea = 75;
            p->pot_maintain_cielo = 50;
            p->pot_maintain_platea = 45;
            p->boost_time = 0;
            break;

        case 14: // LASAGNE - cielo forte per gratinatura
            strncpy(p->name, "LASAGNE", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 200;
            p->phases[0].temp_platea = 180;
            p->phases[0].duration = 25;
            p->phases[0].power_cielo = 80;
            p->phases[0].power_platea = 45;
            p->pot_preheat_cielo = 85;
            p->pot_preheat_platea = 75;
            p->pot_maintain_cielo = 55;
            p->pot_maintain_platea = 45;
            p->boost_time = 0;
            break;

        case 15: // VERDURE - gratinate
            strncpy(p->name, "VERDURE", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 200;
            p->phases[0].temp_platea = 190;
            p->phases[0].duration = 25;
            p->phases[0].power_cielo = 70;
            p->phases[0].power_platea = 55;
            p->pot_preheat_cielo = 80;
            p->pot_preheat_platea = 75;
            p->pot_maintain_cielo = 50;
            p->pot_maintain_platea = 45;
            p->boost_time = 0;
            break;

        case 16: // GRATIN - cielo molto dominante per doratura intensa
            strncpy(p->name, "GRATIN", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 230;
            p->phases[0].temp_platea = 180;
            p->phases[0].duration = 15;
            p->phases[0].power_cielo = 90;
            p->phases[0].power_platea = 35;
            p->pot_preheat_cielo = 90;
            p->pot_preheat_platea = 70;
            p->pot_maintain_cielo = 60;
            p->pot_maintain_platea = 40;
            p->boost_time = 0;
            break;

        case 17: // BRUSCHETTA - tostatura forte da sopra
            strncpy(p->name, "BRUSCHETTA", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 250;
            p->phases[0].temp_platea = 200;
            p->phases[0].duration = 5;
            p->phases[0].power_cielo = 90;
            p->phases[0].power_platea = 40;
            p->pot_preheat_cielo = 90;
            p->pot_preheat_platea = 75;
            p->pot_maintain_cielo = 65;
            p->pot_maintain_platea = 45;
            p->boost_time = 0;
            break;

        case 18: // PESCE - cottura delicata, bilanciata
            strncpy(p->name, "PESCE", MAX_PROGRAM_NAME - 1);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 190;
            p->phases[0].temp_platea = 180;
            p->phases[0].duration = 20;
            p->phases[0].power_cielo = 55;
            p->phases[0].power_platea = 50;
            p->pot_preheat_cielo = 80;
            p->pot_preheat_platea = 75;
            p->pot_maintain_cielo = 50;
            p->pot_maintain_platea = 40;
            p->boost_time = 0;
            break;

        default:
            // P19: programma vuoto
            snprintf(p->name, MAX_PROGRAM_NAME, "PROG. %d", index);
            p->num_phases = 1;
            p->phases[0].temp_cielo = 200;
            p->phases[0].temp_platea = 200;
            p->phases[0].duration = 5;
            p->phases[0].power_cielo = 65;
            p->phases[0].power_platea = 65;
            break;
    }
}

// ============================================
// Chiave NVS per programma
// ============================================
static void get_program_key(uint8_t index, char* key) {
    sprintf(key, "p%02d", index);
}

// ============================================
// Initialize programs storage
// ============================================
void programs_storage_init() {
    Serial.println("Programs: Inizializzazione...");

    s_prefs.begin(PREFS_PROGRAMS_NS, true);  // read-only
    s_current_program = s_prefs.getUChar(KEY_CURRENT, PROGRAM_MANUAL);
    uint8_t stored_version = s_prefs.getUChar(KEY_VERSION, 0);
    s_prefs.end();

    // Se la versione in NVS non corrisponde, forza ricreazione di tutti i default
    bool force_defaults = (stored_version != PROGRAMS_VERSION);
    if (force_defaults) {
        Serial.printf("Programs: Versione cambiata (%d -> %d), ricreo default\n",
                      stored_version, PROGRAMS_VERSION);
    }

    // Carica ogni slot da NVS; ricrea se versione cambiata o caricamento fallito
    uint8_t loaded = 0, created = 0;
    for (uint8_t i = 0; i < MAX_PROGRAMS; i++) {
        if (!force_defaults && programs_load(i)) {
            loaded++;
        } else {
            set_default_program(i);
            programs_save(i);
            created++;
        }
    }

    // Aggiorna flag init e versione
    s_prefs.begin(PREFS_PROGRAMS_NS, false);
    s_prefs.putBool(KEY_INIT, true);
    s_prefs.putUChar(KEY_VERSION, PROGRAMS_VERSION);
    s_prefs.end();

    Serial.printf("Programs: %d caricati, %d creati default\n", loaded, created);

    // All'avvio carica SEMPRE P0 (Manuale)
    s_current_program = PROGRAM_MANUAL;
    programs_load_to_state(PROGRAM_MANUAL);
    Serial.println("Programs: Caricato P0 MANUALE (default avvio)");
}

// ============================================
// Validazione di un programma (da NVS o dal web)
// Valori fuori scala andrebbero dritti nel termostato; nel nome virgolette e
// caratteri di controllo rompono il JSON di /api/status (dashboard e app
// risultano offline) e il font LVGL non li ha comunque
// ============================================
static int clamp_int(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

void programs_sanitize(Program* p) {
    p->name[MAX_PROGRAM_NAME - 1] = '\0';
    for (int c = 0; p->name[c]; c++) {
        char ch = p->name[c];
        if (ch == '"') {
            p->name[c] = '\'';
        } else if (ch == '\\') {
            p->name[c] = '/';
        } else if (ch < 0x20 || ch > 0x7E) {
            p->name[c] = ' ';
        }
    }

    if (p->num_phases < 1 || p->num_phases > MAX_PHASES) p->num_phases = 1;
    for (int f = 0; f < MAX_PHASES; f++) {
        Phase& ph = p->phases[f];
        ph.temp_cielo = clamp_int(ph.temp_cielo, 0, 500);
        ph.temp_platea = clamp_int(ph.temp_platea, 0, 500);
        ph.duration = clamp_int(ph.duration, 0, 99);
        ph.power_cielo = clamp_int(ph.power_cielo, 0, 100);
        ph.power_platea = clamp_int(ph.power_platea, 0, 100);
    }
    if (p->boost_time > 180) p->boost_time = 180;
}

// ============================================
// Load single program from NVS
// ============================================
bool programs_load(uint8_t index) {
    if (index >= MAX_PROGRAMS) return false;

    char key[8];
    get_program_key(index, key);

    s_prefs.begin(PREFS_PROGRAMS_NS, true);
    size_t len = s_prefs.getBytesLength(key);

    if (len == sizeof(Program)) {
        s_prefs.getBytes(key, &g_programs[index], sizeof(Program));
        s_prefs.end();
        programs_sanitize(&g_programs[index]);
        return true;
    }

    s_prefs.end();
    return false;
}

// ============================================
// Save single program to NVS
// ============================================
bool programs_save(uint8_t index) {
    if (index >= MAX_PROGRAMS) return false;

    char key[8];
    get_program_key(index, key);

    s_prefs.begin(PREFS_PROGRAMS_NS, false);
    size_t written = s_prefs.putBytes(key, &g_programs[index], sizeof(Program));
    s_prefs.end();

    Serial.printf("Programs: Salvato P%d (%d bytes)\n", index, written);
    return (written == sizeof(Program));
}

// ============================================
// Get current program index
// ============================================
uint8_t programs_get_current() {
    return s_current_program;
}

// ============================================
// Set current program and load to state
// ============================================
void programs_set_current(uint8_t index) {
    if (index >= MAX_PROGRAMS) return;

    s_current_program = index;

    // Salva su NVS
    s_prefs.begin(PREFS_PROGRAMS_NS, false);
    s_prefs.putUChar(KEY_CURRENT, index);
    s_prefs.end();

    // Carica in g_state
    programs_load_to_state(index);

    Serial.printf("Programs: Selezionato P%d '%s'\n", index, g_programs[index].name);
}

// ============================================
// Load program to g_state operative variables
// ============================================
void programs_load_to_state(uint8_t index) {
    if (index >= MAX_PROGRAMS) return;

    Program* p = &g_programs[index];

    // Info programma
    g_state.program_number = index;
    strncpy(g_state.program_name, p->name, MAX_PROGRAM_NAME - 1);
    g_state.program_name[MAX_PROGRAM_NAME - 1] = '\0';
    g_state.num_phases = p->num_phases;
    g_state.active_phase = 0;

    // Copia fasi
    for (int i = 0; i < MAX_PHASES; i++) {
        g_state.phases[i] = p->phases[i];
    }

    // Preheat e maintain: gestiti automaticamente da get_effective_power()
    // (preheat sempre 100%, maintain da calibrazione)

    // Boost
    g_state.boost_max_time = p->boost_time;

    // Carica parametri fase 0 attiva
    if (p->num_phases > 0) {
        g_state.target_cielo = p->phases[0].temp_cielo;
        g_state.target_platea = p->phases[0].temp_platea;
        g_state.power_cielo = p->phases[0].power_cielo;
        g_state.power_platea = p->phases[0].power_platea;
        g_state.timer_minutes = p->phases[0].duration;
        g_state.timer_seconds = (index == PROGRAM_MANUAL) ? settings_load_p0_timer_sec() : 0;
    }
}

// ============================================
// Reset single program to factory default
// ============================================
void programs_reset_single(uint8_t index) {
    if (index >= MAX_PROGRAMS) return;

    set_default_program(index);
    programs_save(index);

    // Se è il programma corrente, ricarica
    if (index == s_current_program) {
        programs_load_to_state(index);
    }

    Serial.printf("Programs: P%d resettato\n", index);
}

// ============================================
// Factory reset all programs
// ============================================
void programs_factory_reset() {
    Serial.println("Programs: FACTORY RESET");

    for (uint8_t i = 0; i < MAX_PROGRAMS; i++) {
        set_default_program(i);
        programs_save(i);
    }

    // Reset al programma manuale
    s_current_program = PROGRAM_MANUAL;
    s_prefs.begin(PREFS_PROGRAMS_NS, false);
    s_prefs.putUChar(KEY_CURRENT, PROGRAM_MANUAL);
    s_prefs.end();

    programs_load_to_state(PROGRAM_MANUAL);

    Serial.println("Programs: Factory reset completato");
}

// ============================================
// Get short description for program list
// ============================================
void programs_get_description(uint8_t index, char* buf, size_t len) {
    if (index >= MAX_PROGRAMS || !buf || len == 0) return;

    Program* p = &g_programs[index];

    if (p->num_phases > 0 && p->phases[0].temp_cielo > 0) {
        // Formato: "1F 380/350 1:00"
        snprintf(buf, len, "%dF %d/%d %d:%02d",
                 p->num_phases,
                 p->phases[0].temp_cielo,
                 p->phases[0].temp_platea,
                 p->phases[0].duration,
                 0);  // secondi sempre 0 nel display
    } else {
        strncpy(buf, "--", len);
    }
}

// ============================================
// Check if program is configured (not empty)
// ============================================
bool programs_is_configured(uint8_t index) {
    if (index >= MAX_PROGRAMS) return false;

    Program* p = &g_programs[index];
    return (p->num_phases > 0 && p->phases[0].temp_cielo > 0);
}

// ============================================
// Save current g_state params to P0 (Manual)
// ============================================
void programs_save_manual_params() {
    Program* p = &g_programs[PROGRAM_MANUAL];

    // Copia parametri dalla fase attiva (sempre fase 0 per manuale)
    p->phases[0].temp_cielo = g_state.target_cielo;
    p->phases[0].temp_platea = g_state.target_platea;
    p->phases[0].duration = g_state.timer_minutes;
    settings_save_p0_timer_sec(g_state.timer_seconds);
    p->phases[0].power_cielo = g_state.power_cielo;
    p->phases[0].power_platea = g_state.power_platea;

    // Potenze preriscaldamento e mantenimento FISSE per P0
    p->pot_preheat_cielo = 100;
    p->pot_preheat_platea = 100;
    p->pot_maintain_cielo = 95;
    p->pot_maintain_platea = 95;

    // Boost
    p->boost_time = g_state.boost_max_time;

    // La fase in g_state (archi PRO, web) deve coincidere con quella salvata
    g_state.phases[0] = p->phases[0];

    // Salva su NVS
    programs_save(PROGRAM_MANUAL);

    Serial.printf("Programs: P0 salvato (T:%d/%d P:%d/%d)\n",
                 p->phases[0].temp_cielo, p->phases[0].temp_platea,
                 p->phases[0].power_cielo, p->phases[0].power_platea);
}

// ============================================
// Save current phase params to loaded program
// ============================================
void programs_save_current_phase_params() {
    uint8_t prog_idx = g_state.program_number;
    uint8_t phase_idx = g_state.active_phase;

    if (prog_idx >= MAX_PROGRAMS) return;
    if (phase_idx >= g_state.num_phases) return;

    Program* p = &g_programs[prog_idx];

    // Sync num_phases from g_state (may have been added/removed from main screen)
    p->num_phases = g_state.num_phases;

    // Copy all phases from g_state to program
    for (int i = 0; i < MAX_PHASES; i++) {
        p->phases[i] = g_state.phases[i];
    }

    // Copy active phase params from live state
    p->phases[phase_idx].temp_cielo = g_state.target_cielo;
    p->phases[phase_idx].temp_platea = g_state.target_platea;
    p->phases[phase_idx].duration = g_state.timer_minutes;
    p->phases[phase_idx].power_cielo = g_state.power_cielo;
    p->phases[phase_idx].power_platea = g_state.power_platea;

    // Sync back to g_state.phases so UI gauges update immediately
    g_state.phases[phase_idx] = p->phases[phase_idx];

    // Boost
    p->boost_time = g_state.boost_max_time;

    // Persist to NVS
    programs_save(prog_idx);

    Serial.printf("Programs: P%d fase %d salvata (T:%d/%d P:%d/%d D:%d)\n",
                 prog_idx, phase_idx,
                 p->phases[phase_idx].temp_cielo, p->phases[phase_idx].temp_platea,
                 p->phases[phase_idx].power_cielo, p->phases[phase_idx].power_platea,
                 p->phases[phase_idx].duration);
}

// ============================================
// Build JSON array of all programs
// ============================================
size_t programs_build_json(char* buf, size_t buf_size) {
    size_t pos = 0;
    if (buf_size == 0) return 0;
    // snprintf restituisce quanto avrebbe scritto: a buffer pieno buf_size - pos
    // andrebbe in underflow e la scrittura successiva uscirebbe dal buffer
#define JSON_APPEND(...) do { \
        int n_ = snprintf(buf + pos, buf_size - pos, __VA_ARGS__); \
        if (n_ < 0 || (size_t)n_ >= buf_size - pos) { buf[buf_size - 1] = '\0'; return buf_size - 1; } \
        pos += (size_t)n_; \
    } while (0)

    JSON_APPEND("[");

    for (uint8_t i = 0; i < MAX_PROGRAMS; i++) {
        Program* p = &g_programs[i];
        if (i > 0) JSON_APPEND(",");

        JSON_APPEND("{\"i\":%d,\"n\":\"", i);

        // Escape nome (i nomi sono gia' ripuliti da programs_sanitize)
        for (int c = 0; p->name[c] && c < MAX_PROGRAM_NAME - 1; c++) {
            char ch = p->name[c];
            if (ch == '"' || ch == '\\') {
                JSON_APPEND("\\%c", ch);
            } else {
                JSON_APPEND("%c", ch);
            }
        }

        JSON_APPEND("\",\"np\":%d,\"p\":[", p->num_phases);

        for (int f = 0; f < MAX_PHASES; f++) {
            if (f > 0) JSON_APPEND(",");
            JSON_APPEND("{\"tc\":%d,\"tp\":%d,\"d\":%d,\"pc\":%d,\"pp\":%d}",
                p->phases[f].temp_cielo, p->phases[f].temp_platea,
                p->phases[f].duration,
                p->phases[f].power_cielo, p->phases[f].power_platea);
        }

        JSON_APPEND("],\"bt\":%d}", p->boost_time);
    }

    JSON_APPEND("]");
#undef JSON_APPEND
    return pos;
}

