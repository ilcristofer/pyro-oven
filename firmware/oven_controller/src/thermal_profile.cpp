/**
 * Thermal Profile - Implementazione auto-learning termico
 * Impara dal funzionamento normale: rate di riscaldamento per banda,
 * overshoot per target range, potenza maintain per target range.
 * Salva su NVS in momenti chiave per sopravvivere a crash/riavvii.
 */

#include "thermal_profile.h"
#include "oven_state.h"
#include "settings_storage.h"
#include <Preferences.h>
#include <math.h>

// ============================================
// NVS
// ============================================
#define THERM_NVS_NAMESPACE "therm"
#define THERM_KEY_PROFILE   "prof"     // v1: SOLO lettura, mai riscritta
#define THERM_KEY_PROFILE2  "prof2"    // v2: solo lettura (migrazione)
#define THERM_KEY_PROFILE3  "prof3"    // v3: solo lettura (migrazione)
#define THERM_KEY_PROFILE4  "prof4"    // v4: solo lettura (migrazione)
#define THERM_KEY_PROFILE5  "prof5"    // v5: solo lettura (migrazione)
#define THERM_KEY_PROFILE6  "prof6"    // v6: chiave corrente
#define THERM_KEY_INIT      "init"

// Oltre questo un "overshoot" non e' inerzia del forno ma una misura sbagliata
// (riaccensione a caldo, target abbassato): scartato invece di impararlo
#define OVERSHOOT_PLAUSIBLE_MAX 40.0f

// ============================================
// Band boundaries (C)
// ============================================
static const int BAND_LIMITS[] = {100, 200, 300, 400, 999};

static int get_band(int temp) {
    for (int i = 0; i < TEMP_BANDS; i++) {
        if (temp < BAND_LIMITS[i]) return i;
    }
    return TEMP_BANDS - 1;
}

static int get_target_range(int target) {
    if (target < 250) return 0;
    if (target <= 350) return 1;
    return 2;
}

// ============================================
// State
// ============================================
static ThermalProfile s_profile;
static Preferences s_prefs;

// Ring buffer per rate real-time. Ogni campione porta il proprio istante:
// il tick nominale e' 2s, ma il loop puo' ritardare e l'intervallo vero e'
// l'unico modo per avere gradi/minuto giusti.
static float s_temp_history_cielo[RATE_HISTORY_SIZE];
static float s_temp_history_platea[RATE_HISTORY_SIZE];
static unsigned long s_time_history[RATE_HISTORY_SIZE];
// Potenza effettiva applicata a ogni campione: in preriscaldamento puo' essere
// ridotta, e la velocita' misurata allora non e' quella a piena potenza
static uint8_t s_power_history_cielo[RATE_HISTORY_SIZE];
static uint8_t s_power_history_platea[RATE_HISTORY_SIZE];
static int s_history_idx = 0;
static int s_history_count = 0;

// Session rate tracking per banda. Cielo e platea attraversano le bande in
// momenti diversi: ogni elemento ha i suoi contatori, altrimenti la media di
// uno viene divisa per i campioni dell'altro.
// Si accumulano gradi e minuti, non la media dei rate istantanei: la velocita'
// di una banda e' gradi totali diviso tempo totale. Mediare i rate pesa per
// tempo e tira in basso il risultato quando dentro la banda la potenza cambia.
static float s_session_deg_cielo[TEMP_BANDS];
static float s_session_min_cielo[TEMP_BANDS];
static float s_session_deg_platea[TEMP_BANDS];
static float s_session_min_platea[TEMP_BANDS];
static int s_session_rate_count_cielo[TEMP_BANDS];
static int s_session_rate_count_platea[TEMP_BANDS];
// Temperatura a cui i rate della sessione sono stati effettivamente misurati
static float s_session_temp_sum_cielo[TEMP_BANDS];
static float s_session_temp_sum_platea[TEMP_BANDS];
static int s_session_temp_count_cielo[TEMP_BANDS];
static int s_session_temp_count_platea[TEMP_BANDS];

// Overshoot tracking
static bool s_tracking_overshoot_cielo = false;
static bool s_tracking_overshoot_platea = false;
static float s_peak_cielo = 0;
static float s_peak_platea = 0;
static unsigned long s_peak_ms_cielo = 0;
static unsigned long s_peak_ms_platea = 0;
static bool s_overshoot_done_cielo = false;
static bool s_overshoot_done_platea = false;

// Maintain autotune
static int s_current_maintain_cielo = DEFAULT_MAINTAIN;
static int s_current_maintain_platea = DEFAULT_MAINTAIN;
static unsigned long s_last_maintain_check_ms = 0;
static unsigned long s_last_maintain_save_ms = 0;
static float s_maintain_temp_sum_cielo = 0;
static float s_maintain_temp_sum_platea = 0;
static int s_maintain_temp_count = 0;
static unsigned long s_stable_start_cielo = 0;
static unsigned long s_stable_start_platea = 0;
static bool s_profile_dirty = false;  // dati nuovi non salvati
// L'autotune resta fermo finche' l'elemento non e' rientrato al target: sulla
// coda dell'overshoot leggerebbe "mantenimento troppo alto" e abbasserebbe la
// potenza per un errore che non dipende da lei.
static bool s_back_to_target_cielo = false;
static bool s_back_to_target_platea = false;

// Soak: saturazione della pietra dopo che le sonde sono a target
static float s_soak_fraction = 1.0f;        // quota di soak necessaria (pietra gia' calda -> <1)
// Il duty si misura su cicli interi del termostato (da un'accensione della
// platea alla successiva, ~4 min a 235 C). Finestre fisse da 60s ne vedevano
// solo un pezzo: il duty saltava tra 0 e la potenza piena e l'assestamento
// veniva riconosciuto per caso (sessione 27/9: 21 min invece di ~15).
static float s_duty_sum_platea = 0;
static int s_duty_count = 0;
static bool s_duty_in_cycle = false;        // false fino alla prima accensione
static bool s_duty_prev_on = false;
static float s_duty_prev_cycle = -1.0f;     // duty del ciclo precedente (-1 = nessuno)
static int s_duty_settle_cycles = 0;
static unsigned long s_duty_last_ms = 0;    // ultimo campione: un buco rompe il ciclo
static unsigned long s_duty_cycle_ms = 0;   // inizio del ciclo in corso

static void soak_cycle_reset() {
    s_duty_sum_platea = 0;
    s_duty_count = 0;
    s_duty_in_cycle = false;
    s_duty_prev_on = false;
    s_duty_prev_cycle = -1.0f;
    s_duty_settle_cycles = 0;
    s_duty_last_ms = 0;
    s_duty_cycle_ms = 0;
}

// Bias ETA: il forno impara di quanto sbaglia la propria previsione di salita
static unsigned long s_climb_start_ms = 0;
static float s_climb_predicted_min = 0;
static bool s_session_valid = true;         // false se un evento ha falsato le misure

// ETA mostrato: smorzato e monotono, altrimenti il numero balla
static int s_eta_displayed = -1;
static unsigned long s_eta_last_update_ms = 0;

// Previous state tracking
static OvenMode s_prev_mode = OvenMode::STANDBY;
static bool s_prev_cielo_in_maintain = false;
static bool s_prev_platea_in_maintain = false;

// ============================================
// Forward declarations (definite piu' sotto, usate da record())
// ============================================
static float calc_climb_raw();
static float soak_fraction_from(int start_platea, int target_platea);

// ============================================
// Layout v1: serve solo a migrare un profilo salvato dalle versioni
// precedenti senza perdere quello che il forno ha gia' imparato.
// I campi v2 sono aggiunti IN CODA, quindi gli offset dei v1 coincidono.
// ============================================
struct ThermalProfileV1 {
    uint8_t version;
    float rate_cielo[TEMP_BANDS];
    float rate_platea[TEMP_BANDS];
    uint16_t rate_sessions[TEMP_BANDS];
    float overshoot_cielo[TARGET_RANGES];
    float overshoot_platea[TARGET_RANGES];
    uint16_t overshoot_sessions[TARGET_RANGES];
    uint8_t maintain_cielo[TARGET_RANGES];
    uint8_t maintain_platea[TARGET_RANGES];
    uint16_t maintain_sessions[TARGET_RANGES];
};

// Layout v2 (profilo scritto dal firmware del 2026-09-16, prima che venisse
// introdotta la temperatura media per banda)
struct ThermalProfileV2 {
    ThermalProfileV1 v1;
    float soak_minutes[TARGET_RANGES];
    uint16_t soak_sessions[TARGET_RANGES];
    float eta_bias[TARGET_RANGES];
    uint16_t eta_bias_sessions[TARGET_RANGES];
};

// Default dei soli campi introdotti in v3
static void init_v3_defaults() {
    for (int b = 0; b < TEMP_BANDS; b++) {
        s_profile.rate_temp_cielo[b] = 0.0f;   // 0 = usa il centro banda
        s_profile.rate_temp_platea[b] = 0.0f;
    }
}

// Default dei soli campi introdotti in v2
static void init_v2_defaults() {
    const float soak_def[TARGET_RANGES] = {
        (float)DEFAULT_SOAK_LOW, (float)DEFAULT_SOAK_MID, (float)DEFAULT_SOAK_HIGH
    };
    for (int i = 0; i < TARGET_RANGES; i++) {
        s_profile.soak_minutes[i] = soak_def[i];
        s_profile.soak_sessions[i] = 0;
        s_profile.eta_bias[i] = 1.0f;
        s_profile.eta_bias_sessions[i] = 0;
    }
    init_v3_defaults();
}

// Migra in RAM un profilo v1 salvato su NVS. true = migrato.
static bool try_migrate_v1() {
    if (s_prefs.getBytesLength(THERM_KEY_PROFILE) != sizeof(ThermalProfileV1)) return false;

    ThermalProfileV1 v1;
    if (s_prefs.getBytes(THERM_KEY_PROFILE, &v1, sizeof(v1)) != sizeof(v1)) return false;
    if (v1.version != 1) return false;

    memset(&s_profile, 0, sizeof(ThermalProfile));
    memcpy(&s_profile, &v1, sizeof(v1));
    s_profile.version = THERMAL_PROFILE_VER;
    init_v2_defaults();   // include i default v3
    Serial.println("ThermalProfile: profilo v1 migrato (dati appresi conservati)");
    return true;
}

// Migra in RAM un profilo v2. true = migrato.
static bool try_migrate_v2() {
    if (s_prefs.getBytesLength(THERM_KEY_PROFILE2) != sizeof(ThermalProfileV2)) return false;

    ThermalProfileV2 v2;
    if (s_prefs.getBytes(THERM_KEY_PROFILE2, &v2, sizeof(v2)) != sizeof(v2)) return false;
    if (v2.v1.version != 2) return false;

    memset(&s_profile, 0, sizeof(ThermalProfile));
    memcpy(&s_profile, &v2, sizeof(v2));
    s_profile.version = THERMAL_PROFILE_VER;
    init_v3_defaults();
    Serial.println("ThermalProfile: profilo v2 migrato (dati appresi conservati)");
    return true;
}

// Migra un profilo v3. Il formato e' identico: cambia cosa si puo' credere.
// Fino a v3 il soak veniva dichiarato raggiunto anche quando la platea era
// sopra il target (resistenza spenta, duty fermo a zero) e i rate della platea
// venivano divisi per i campioni del cielo. I valori nati da quelle regole
// tornano ai default; gli altri restano, ma con il contatore abbassato cosi'
// che le prossime misure pesino la meta' invece di un decimo.
static bool try_migrate_v3() {
    if (s_prefs.getBytesLength(THERM_KEY_PROFILE3) != sizeof(ThermalProfile)) return false;

    ThermalProfile v3;
    if (s_prefs.getBytes(THERM_KEY_PROFILE3, &v3, sizeof(v3)) != sizeof(v3)) return false;
    if (v3.version != 3) return false;

    memcpy(&s_profile, &v3, sizeof(v3));
    s_profile.version = THERMAL_PROFILE_VER;

    const float soak_def[TARGET_RANGES] = {
        (float)DEFAULT_SOAK_LOW, (float)DEFAULT_SOAK_MID, (float)DEFAULT_SOAK_HIGH
    };
    for (int i = 0; i < TARGET_RANGES; i++) {
        s_profile.soak_minutes[i] = soak_def[i];
        s_profile.soak_sessions[i] = 0;
        s_profile.eta_bias[i] = 1.0f;
        s_profile.eta_bias_sessions[i] = 0;
        if (s_profile.overshoot_sessions[i] > 1) s_profile.overshoot_sessions[i] = 1;
    }
    for (int b = 0; b < TEMP_BANDS; b++) {
        if (s_profile.rate_sessions[b] > 1) s_profile.rate_sessions[b] = 1;
    }

    Serial.println("ThermalProfile: profilo v3 migrato (soak e bias ripartono dai default)");
    return true;
}

// Migra un profilo v4. Fino a v4 la velocita' veniva misurata su 3 intervalli
// e divisa per 4: ogni rate imparato e' un quarto sotto il vero. I valori
// restano (sono comunque un punto di partenza) ma il contatore scende a 1,
// cosi' la prima misura fatta bene pesa la meta'. La correzione ETA riparte
// da 1.00 perche' era stata imparata contro il modello sbagliato: con i rate
// giusti la previsione cambia e quel numero non vuole piu' dire niente.
static bool try_migrate_v4() {
    if (s_prefs.getBytesLength(THERM_KEY_PROFILE4) != sizeof(ThermalProfile)) return false;

    ThermalProfile v4;
    if (s_prefs.getBytes(THERM_KEY_PROFILE4, &v4, sizeof(v4)) != sizeof(v4)) return false;
    if (v4.version != 4) return false;

    memcpy(&s_profile, &v4, sizeof(v4));
    s_profile.version = THERMAL_PROFILE_VER;

    for (int b = 0; b < TEMP_BANDS; b++) {
        if (s_profile.rate_sessions[b] > 1) s_profile.rate_sessions[b] = 1;
    }
    for (int i = 0; i < TARGET_RANGES; i++) {
        s_profile.eta_bias[i] = 1.0f;
        s_profile.eta_bias_sessions[i] = 0;
    }

    Serial.println("ThermalProfile: profilo v4 migrato (rate da riverificare, bias azzerato)");
    return true;
}

// Migra un profilo v5. Fino a v5 in preriscaldamento si imparava la velocita'
// anche dell'elemento rallentato per aspettare l'altro (fino al 55% di
// potenza): i rate sono sotto il vero, soprattutto quelli della platea sopra
// i 200 C. Restano come punto di partenza, ma il contatore scende a 1 perche'
// la prima misura a piena potenza pesi la meta'. Azzerarli spegnerebbe il
// modello esponenziale, che usa solo le bande imparate. Il bias ETA era stato
// imparato contro quel modello lento e riparte da 1.00.
static bool try_migrate_v5() {
    if (s_prefs.getBytesLength(THERM_KEY_PROFILE5) != sizeof(ThermalProfile)) return false;

    ThermalProfile v5;
    if (s_prefs.getBytes(THERM_KEY_PROFILE5, &v5, sizeof(v5)) != sizeof(v5)) return false;
    if (v5.version != 5) return false;

    memcpy(&s_profile, &v5, sizeof(v5));
    s_profile.version = THERMAL_PROFILE_VER;

    for (int b = 0; b < TEMP_BANDS; b++) {
        if (s_profile.rate_sessions[b] > 1) s_profile.rate_sessions[b] = 1;
    }
    for (int i = 0; i < TARGET_RANGES; i++) {
        s_profile.eta_bias[i] = 1.0f;
        s_profile.eta_bias_sessions[i] = 0;
    }

    Serial.println("ThermalProfile: profilo v5 migrato (rate da riverificare a piena potenza, bias azzerato)");
    return true;
}

// ============================================
// Helper: inizializza profilo con default
// ============================================
static void init_defaults() {
    memset(&s_profile, 0, sizeof(ThermalProfile));
    s_profile.version = THERMAL_PROFILE_VER;

    // Rate iniziali per banda calcolati dal modello misurato sul forno reale:
    // rate = (T_inf - T_media_banda) / tau. Molto piu' vicini al vero di un
    // valore unico per tutte le bande, che sovrastimava in alto e sottostimava
    // in basso. Vengono comunque sostituiti da quanto il forno impara.
    static const float RATE0_CIELO[TEMP_BANDS]  = {22.4f, 17.3f, 11.4f, 5.5f, 2.0f};
    static const float RATE0_PLATEA[TEMP_BANDS] = {28.0f, 20.0f, 11.8f, 4.0f, 2.0f};

    for (int i = 0; i < TEMP_BANDS; i++) {
        s_profile.rate_cielo[i] = RATE0_CIELO[i];
        s_profile.rate_platea[i] = RATE0_PLATEA[i];
        s_profile.rate_sessions[i] = 0;
    }

    for (int i = 0; i < TARGET_RANGES; i++) {
        s_profile.overshoot_cielo[i] = DEFAULT_OVERSHOOT_CIELO;
        s_profile.overshoot_platea[i] = DEFAULT_OVERSHOOT_PLATEA;
        s_profile.overshoot_sessions[i] = 0;

        s_profile.maintain_cielo[i] = DEFAULT_MAINTAIN;
        s_profile.maintain_platea[i] = DEFAULT_MAINTAIN;
        s_profile.maintain_sessions[i] = 0;
    }

    init_v2_defaults();
}

// ============================================
// Helper: reset session state
// ============================================
static void reset_session_state() {
    s_history_idx = 0;
    s_history_count = 0;

    memset(s_session_deg_cielo, 0, sizeof(s_session_deg_cielo));
    memset(s_session_min_cielo, 0, sizeof(s_session_min_cielo));
    memset(s_session_deg_platea, 0, sizeof(s_session_deg_platea));
    memset(s_session_min_platea, 0, sizeof(s_session_min_platea));
    memset(s_session_rate_count_cielo, 0, sizeof(s_session_rate_count_cielo));
    memset(s_session_rate_count_platea, 0, sizeof(s_session_rate_count_platea));
    memset(s_session_temp_sum_cielo, 0, sizeof(s_session_temp_sum_cielo));
    memset(s_session_temp_sum_platea, 0, sizeof(s_session_temp_sum_platea));
    memset(s_session_temp_count_cielo, 0, sizeof(s_session_temp_count_cielo));
    memset(s_session_temp_count_platea, 0, sizeof(s_session_temp_count_platea));
    s_tracking_overshoot_cielo = false;
    s_tracking_overshoot_platea = false;
    s_overshoot_done_cielo = false;
    s_overshoot_done_platea = false;
    s_peak_ms_cielo = 0;
    s_peak_ms_platea = 0;

    s_last_maintain_check_ms = 0;
    s_last_maintain_save_ms = 0;
    s_maintain_temp_sum_cielo = 0;
    s_maintain_temp_sum_platea = 0;
    s_maintain_temp_count = 0;
    s_stable_start_cielo = 0;
    s_stable_start_platea = 0;
    s_back_to_target_cielo = false;
    s_back_to_target_platea = false;
    s_profile_dirty = false;

    s_prev_mode = OvenMode::STANDBY;
    s_prev_cielo_in_maintain = false;
    s_prev_platea_in_maintain = false;

    s_soak_fraction = 1.0f;
    soak_cycle_reset();

    s_climb_start_ms = 0;
    s_climb_predicted_min = 0;
    s_session_valid = true;

    s_eta_displayed = -1;
    s_eta_last_update_ms = 0;

    g_state.stone_ready = false;
    g_state.soak_start_ms = 0;
}

// ============================================
// Helper: EMA blend (cap sessioni a 10)
// ============================================
static float ema_blend(float old_val, float new_val, uint16_t sessions) {
    if (sessions == 0) return new_val;
    float alpha = 1.0f / (float)(sessions < 10 ? sessions + 1 : 10);
    return old_val * (1.0f - alpha) + new_val * alpha;
}

// Variante per i valori che partono da un default sensato (soak, bias ETA):
// la prima misura pesa la meta' invece di sostituire tutto. Una sola lettura
// sfortunata cancellava il punto di partenza.
static float ema_blend_soft(float old_val, float new_val, uint16_t sessions) {
    if (sessions == 0) return (old_val + new_val) * 0.5f;
    return ema_blend(old_val, new_val, sessions);
}

// ============================================
// Helper: salva rate appresi per bande con dati
// ============================================
// Una banda viene imparata solo con abbastanza campioni: le briciole raccolte
// appena entrati in una banda non dicono nulla sulla banda, ma entravano nella
// media con lo stesso peso di una misura completa.
// I due elementi si salvano quando ciascuno finisce la propria salita: salvare
// anche l'altro lascerebbe in archivio la sua codina lenta appena prima del
// mantenimento come se fosse una banda intera.
static void save_session_rates(bool do_cielo, bool do_platea) {
    bool saved_any = false;
    for (int b = 0; b < TEMP_BANDS; b++) {
        bool have_c = do_cielo && s_session_rate_count_cielo[b] >= RATE_MIN_SAMPLES &&
                      s_session_min_cielo[b] > 0.01f;
        bool have_p = do_platea && s_session_rate_count_platea[b] >= RATE_MIN_SAMPLES &&
                      s_session_min_platea[b] > 0.01f;
        bool saved = false;

        if (have_c) {
            float avg_c = s_session_deg_cielo[b] / s_session_min_cielo[b];
            if (avg_c > 0.5f && avg_c < 50.0f) {
                s_profile.rate_cielo[b] = ema_blend(
                    s_profile.rate_cielo[b], avg_c, s_profile.rate_sessions[b]);

                // Temperatura media a cui quel rate e' stato misurato: e' questa
                // l'ascissa corretta per stimare il modello, non il centro banda
                if (s_session_temp_count_cielo[b] > 0) {
                    float tmed = s_session_temp_sum_cielo[b] / s_session_temp_count_cielo[b];
                    s_profile.rate_temp_cielo[b] = (s_profile.rate_temp_cielo[b] > 1.0f)
                        ? ema_blend(s_profile.rate_temp_cielo[b], tmed, s_profile.rate_sessions[b])
                        : tmed;
                }
                saved = true;
            }
        }

        if (have_p) {
            float avg_p = s_session_deg_platea[b] / s_session_min_platea[b];
            if (avg_p > 0.5f && avg_p < 50.0f) {
                s_profile.rate_platea[b] = ema_blend(
                    s_profile.rate_platea[b], avg_p, s_profile.rate_sessions[b]);

                if (s_session_temp_count_platea[b] > 0) {
                    float tmed = s_session_temp_sum_platea[b] / s_session_temp_count_platea[b];
                    s_profile.rate_temp_platea[b] = (s_profile.rate_temp_platea[b] > 1.0f)
                        ? ema_blend(s_profile.rate_temp_platea[b], tmed, s_profile.rate_sessions[b])
                        : tmed;
                }
                saved = true;
            }
        }

        if (saved) {
            if (s_profile.rate_sessions[b] < 1000) {
                s_profile.rate_sessions[b]++;
            }
            saved_any = true;
        }

        // Quello che era troppo poco per essere imparato viene comunque
        // scartato: e' roba della salita appena conclusa, non della prossima
        if (do_cielo) {
            s_session_deg_cielo[b] = 0;
            s_session_min_cielo[b] = 0;
            s_session_rate_count_cielo[b] = 0;
            s_session_temp_sum_cielo[b] = 0;
            s_session_temp_count_cielo[b] = 0;
        }
        if (do_platea) {
            s_session_deg_platea[b] = 0;
            s_session_min_platea[b] = 0;
            s_session_rate_count_platea[b] = 0;
            s_session_temp_sum_platea[b] = 0;
            s_session_temp_count_platea[b] = 0;
        }
    }
    if (saved_any) {
        Serial.println("ThermalProfile: rate salvati (blend EMA)");
    }
}

// ============================================
// Validazione del profilo letto da NVS
// Un blob corrotto (NaN, maintain a 0 o 255) finirebbe dritto nella potenza
// delle resistenze e nell'ETA: ogni campo viene riportato nel suo intervallo
// ============================================
static float sane_float(float v, float lo, float hi, float fallback) {
    if (!isfinite(v)) return fallback;
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void sanitize_profile() {
    for (int b = 0; b < TEMP_BANDS; b++) {
        s_profile.rate_cielo[b] = sane_float(s_profile.rate_cielo[b], 0.5f, 50.0f, DEFAULT_RATE_CIELO);
        s_profile.rate_platea[b] = sane_float(s_profile.rate_platea[b], 0.5f, 50.0f, DEFAULT_RATE_PLATEA);
        s_profile.rate_temp_cielo[b] = sane_float(s_profile.rate_temp_cielo[b], 0.0f, 600.0f, 0.0f);
        s_profile.rate_temp_platea[b] = sane_float(s_profile.rate_temp_platea[b], 0.0f, 600.0f, 0.0f);
    }
    for (int r = 0; r < TARGET_RANGES; r++) {
        s_profile.overshoot_cielo[r] = sane_float(s_profile.overshoot_cielo[r], 0.0f,
                                                  OVERSHOOT_PLAUSIBLE_MAX, DEFAULT_OVERSHOOT_CIELO);
        s_profile.overshoot_platea[r] = sane_float(s_profile.overshoot_platea[r], 0.0f,
                                                   OVERSHOOT_PLAUSIBLE_MAX, DEFAULT_OVERSHOOT_PLATEA);
        if (s_profile.maintain_cielo[r] < MAINTAIN_MIN || s_profile.maintain_cielo[r] > MAINTAIN_MAX) {
            s_profile.maintain_cielo[r] = DEFAULT_MAINTAIN;
        }
        if (s_profile.maintain_platea[r] < MAINTAIN_MIN || s_profile.maintain_platea[r] > MAINTAIN_MAX) {
            s_profile.maintain_platea[r] = DEFAULT_MAINTAIN;
        }
        s_profile.soak_minutes[r] = sane_float(s_profile.soak_minutes[r], (float)SOAK_MIN_MINUTES,
                                               (float)SOAK_MAX_MINUTES, (float)DEFAULT_SOAK_MID);
        s_profile.eta_bias[r] = sane_float(s_profile.eta_bias[r], ETA_BIAS_MIN, ETA_BIAS_MAX, 1.0f);
    }
}

// ============================================
// Init
// ============================================
void thermal_profile_init() {
    s_prefs.begin(THERM_NVS_NAMESPACE, true);
    bool initialized = s_prefs.getBool(THERM_KEY_INIT, false);

    if (initialized) {
        // Prima la chiave v2; se manca, si migra in RAM dal v1 (la cui chiave
        // resta intatta, cosi' un eventuale ritorno al firmware precedente
        // ritrova il profilo appreso al suo posto)
        size_t len = s_prefs.getBytes(THERM_KEY_PROFILE6, &s_profile, sizeof(ThermalProfile));
        if (len != sizeof(ThermalProfile)) {
            len = (try_migrate_v5() || try_migrate_v4() || try_migrate_v3() || try_migrate_v2() || try_migrate_v1())
                  ? sizeof(ThermalProfile) : 0;
        }
        if (len != sizeof(ThermalProfile) || s_profile.version != THERMAL_PROFILE_VER) {
            Serial.println("ThermalProfile: versione diversa, reset default");
            init_defaults();
        } else {
            sanitize_profile();
            Serial.println("ThermalProfile: profilo caricato da NVS");
            Serial.printf("  Rate cielo band0=%.1f, platea band0=%.1f\n",
                           s_profile.rate_cielo[0], s_profile.rate_platea[0]);
            Serial.printf("  Maintain cielo r0=%d%%, platea r0=%d%%\n",
                           s_profile.maintain_cielo[0], s_profile.maintain_platea[0]);
        }
    } else {
        Serial.println("ThermalProfile: nessun dato, uso default");
        init_defaults();
    }

    s_prefs.end();

    // Inizializza maintain runtime dal profilo
    // Usa range 1 (250-350) come default ragionevole
    s_current_maintain_cielo = s_profile.maintain_cielo[1];
    s_current_maintain_platea = s_profile.maintain_platea[1];

    reset_session_state();
}

// ============================================
// Save
// ============================================
void thermal_profile_save() {
    s_prefs.begin(THERM_NVS_NAMESPACE, false);
    // Scrive solo sulla chiave corrente: le precedenti restano come backup
    // per un eventuale ritorno a un firmware piu' vecchio
    s_prefs.putBytes(THERM_KEY_PROFILE6, &s_profile, sizeof(ThermalProfile));
    s_prefs.putBool(THERM_KEY_INIT, true);
    s_prefs.end();
    s_profile_dirty = false;
    Serial.println("ThermalProfile: salvato su NVS");
}

void thermal_profile_save_if_dirty() {
    if (s_profile_dirty) thermal_profile_save();
}

// ============================================
// Reset
// ============================================
void thermal_profile_reset() {
    init_defaults();
    s_prefs.begin(THERM_NVS_NAMESPACE, false);
    s_prefs.clear();
    s_prefs.end();
    s_current_maintain_cielo = DEFAULT_MAINTAIN;
    s_current_maintain_platea = DEFAULT_MAINTAIN;
    reset_session_state();
    Serial.println("ThermalProfile: RESET");
}

// ============================================
// Record - il cuore, chiamata ogni 2s
// ============================================

// Vero se negli ultimi n campioni l'elemento e' stato sempre a piena potenza
static bool window_full_power(bool is_cielo, int n) {
    if (n > s_history_count) return false;
    const uint8_t* hist = is_cielo ? s_power_history_cielo : s_power_history_platea;
    for (int i = 1; i <= n; i++) {
        int idx = (s_history_idx - i + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
        if (hist[idx] < PREHEAT_FULL_POWER_MIN) return false;
    }
    return true;
}

void thermal_profile_record() {
    if (!g_state.power_on) {
        // Transizione a standby: salva tutto
        if (s_prev_mode != OvenMode::STANDBY) {
            save_session_rates(true, true);
            thermal_profile_save();
            reset_session_state();
        }
        s_prev_mode = OvenMode::STANDBY;
        return;
    }

    float tc = (float)g_state.temp_cielo;
    float tp = (float)g_state.temp_platea;
    unsigned long now = millis();

    // 1. Ring buffer (sempre)
    s_temp_history_cielo[s_history_idx] = tc;
    s_temp_history_platea[s_history_idx] = tp;
    s_time_history[s_history_idx] = now;
    s_power_history_cielo[s_history_idx] = (uint8_t)oven_state_get_effective_power_cielo();
    s_power_history_platea[s_history_idx] = (uint8_t)oven_state_get_effective_power_platea();
    s_history_idx = (s_history_idx + 1) % RATE_HISTORY_SIZE;
    if (s_history_count < RATE_HISTORY_SIZE) s_history_count++;

    // 1a. Cambio di target a forno acceso: quello che era "a regime" non lo e'
    //     piu'. L'autotune deve aspettare il rientro sul nuovo valore, e la
    //     previsione fatta per il vecchio target non va usata per imparare.
    static int s_prev_target_cielo = 0;
    static int s_prev_target_platea = 0;
    if (g_state.target_cielo != s_prev_target_cielo ||
        g_state.target_platea != s_prev_target_platea) {

        // Un overshoot in misura contro il vecchio target verrebbe confrontato
        // con quello nuovo: la misura si abbandona
        if (g_state.target_cielo != s_prev_target_cielo) {
            s_back_to_target_cielo = false;
            s_tracking_overshoot_cielo = false;
            s_current_maintain_cielo =
                s_profile.maintain_cielo[get_target_range(g_state.target_cielo)];
        }
        if (g_state.target_platea != s_prev_target_platea) {
            s_back_to_target_platea = false;
            s_tracking_overshoot_platea = false;
            s_current_maintain_platea =
                s_profile.maintain_platea[get_target_range(g_state.target_platea)];
            // Pietra gia' calda portata un po' piu' su: serve solo la quota di
            // saturazione del salto, non quella di una salita da freddo
            if (s_prev_target_platea != 0 && g_state.target_platea > s_prev_target_platea &&
                !g_state.stone_ready) {
                s_soak_fraction = soak_fraction_from(g_state.temp_platea, g_state.target_platea);
            }
        }
        // Una salita interrotta a meta' da un target nuovo non dice piu' nulla
        // su quanto il forno ci mette: meglio non impararci sopra
        if (s_prev_target_cielo != 0 && s_climb_start_ms > 0 && !g_state.stone_ready) {
            s_session_valid = false;
        }

        s_prev_target_cielo = g_state.target_cielo;
        s_prev_target_platea = g_state.target_platea;
        s_maintain_temp_sum_cielo = 0;
        s_maintain_temp_sum_platea = 0;
        s_maintain_temp_count = 0;
        s_stable_start_cielo = 0;
        s_stable_start_platea = 0;
        s_last_maintain_check_ms = now;
    }

    // 1b. Inizio salita: fissa i riferimenti per imparare il bias dell'ETA e
    //     per decidere quanta saturazione serve (pietra gia' calda -> meno)
    // Il ritorno da un boost non e' una salita nuova: si continua quella di prima
    if (g_state.mode == OvenMode::PREHEAT && s_prev_mode != OvenMode::PREHEAT &&
        !(s_prev_mode == OvenMode::BOOST && s_climb_start_ms > 0)) {
        s_climb_start_ms = now;
        s_climb_predicted_min = calc_climb_raw();
        s_soak_fraction = soak_fraction_from(g_state.temp_platea, g_state.target_platea);
        s_session_valid = true;
        g_state.stone_ready = false;
        g_state.soak_start_ms = 0;
        Serial.printf("ThermalProfile: inizio salita, previsti %.1f min, soak al %.0f%%\n",
                       s_climb_predicted_min, s_soak_fraction * 100.0f);
    }

    // 2. PREHEAT: raccogli rate per banda
    if (g_state.mode == OvenMode::PREHEAT) {
        // Finestra di 4 campioni indietro dal piu' recente, con il tempo vero
        // che li separa (il campione appena scritto sta in s_history_idx - 1)
        int oldest = (s_history_idx - 5 + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
        float win_min = (float)(now - s_time_history[oldest]) / 60000.0f;

        // Rate cielo (solo se non in maintain e a piena potenza su tutta la
        // finestra: rallentato per aspettare la platea salirebbe piu' piano)
        if (!g_state.cielo_in_maintain && s_history_count >= 5 && win_min > 0.01f &&
            window_full_power(true, 5)) {
            float delta = tc - s_temp_history_cielo[oldest];

            if (delta / win_min > 0.5f) {
                int band = get_band((int)tc);
                s_session_deg_cielo[band] += delta;
                s_session_min_cielo[band] += win_min;
                s_session_rate_count_cielo[band]++;
                s_session_temp_sum_cielo[band] += tc;
                s_session_temp_count_cielo[band]++;
            }
        }

        // Rate platea (stesse condizioni)
        if (!g_state.platea_in_maintain && s_history_count >= 5 && win_min > 0.01f &&
            window_full_power(false, 5)) {
            float delta = tp - s_temp_history_platea[oldest];

            if (delta / win_min > 0.5f) {
                int band = get_band((int)tp);
                s_session_deg_platea[band] += delta;
                s_session_min_platea[band] += win_min;
                s_session_rate_count_platea[band]++;
                s_session_temp_sum_platea[band] += tp;
                s_session_temp_count_platea[band]++;
            }
        }
    }

    // 3. Rileva transizione per-elemento a maintain
    // L'overshoot si misura solo arrivando da sotto il target: a una
    // riaccensione a caldo l'elemento e' gia' sopra e il "picco" sarebbe
    // soltanto la temperatura rimasta dalla cottura precedente
    if (g_state.cielo_in_maintain && !s_prev_cielo_in_maintain && !s_overshoot_done_cielo) {
        // Cielo appena passato a maintain
        save_session_rates(true, false);
        s_profile_dirty = true;
        s_tracking_overshoot_cielo = tc < (float)g_state.target_cielo;
        s_peak_cielo = tc;
        s_peak_ms_cielo = now;
        Serial.printf("ThermalProfile: cielo -> maintain, inizio tracking overshoot (%.0f)\n", tc);
    }

    if (g_state.platea_in_maintain && !s_prev_platea_in_maintain && !s_overshoot_done_platea) {
        save_session_rates(false, true);
        s_profile_dirty = true;
        s_tracking_overshoot_platea = tp < (float)g_state.target_platea;
        s_peak_platea = tp;
        s_peak_ms_platea = now;
        Serial.printf("ThermalProfile: platea -> maintain, inizio tracking overshoot (%.0f)\n", tp);
    }

    // Overshoot tracking cielo
    if (s_tracking_overshoot_cielo) {
        if (tc > s_peak_cielo) {
            s_peak_cielo = tc;
            s_peak_ms_cielo = now;
        }

        // Picco confermato quando la temperatura e' scesa di qualche grado
        // oppure quando il massimo regge da un po': con letture intere e un
        // raffreddamento lento non arrivano 3 cali consecutivi.
        // Solo dopo aver toccato il target: sotto non c'e' nessun overshoot da
        // misurare, e in alto la salita finale e' cosi' lenta che l'attesa
        // verrebbe scambiata per un picco.
        if (s_peak_cielo >= (float)g_state.target_cielo &&
            (tc <= s_peak_cielo - OVERSHOOT_CONFIRM_DROP ||
             now - s_peak_ms_cielo >= OVERSHOOT_CONFIRM_MS)) {
            float overshoot = s_peak_cielo - (float)g_state.target_cielo;
            if (overshoot < 0) overshoot = 0;
            int range = get_target_range(g_state.target_cielo);

            if (overshoot <= OVERSHOOT_PLAUSIBLE_MAX) {
                s_profile.overshoot_cielo[range] = ema_blend(
                    s_profile.overshoot_cielo[range], overshoot, s_profile.overshoot_sessions[range]);
                if (s_profile.overshoot_sessions[range] < 1000) {
                    s_profile.overshoot_sessions[range]++;
                }
            }

            s_tracking_overshoot_cielo = false;
            s_overshoot_done_cielo = true;
            s_profile_dirty = true;
            Serial.printf("ThermalProfile: cielo overshoot=%.1f, appreso=%.1f (range %d)\n",
                           overshoot, s_profile.overshoot_cielo[range], range);
        }
    }

    // Overshoot tracking platea
    if (s_tracking_overshoot_platea) {
        if (tp > s_peak_platea) {
            s_peak_platea = tp;
            s_peak_ms_platea = now;
        }

        if (s_peak_platea >= (float)g_state.target_platea &&
            (tp <= s_peak_platea - OVERSHOOT_CONFIRM_DROP ||
             now - s_peak_ms_platea >= OVERSHOOT_CONFIRM_MS)) {
            float overshoot = s_peak_platea - (float)g_state.target_platea;
            if (overshoot < 0) overshoot = 0;
            int range = get_target_range(g_state.target_platea);

            if (overshoot <= OVERSHOOT_PLAUSIBLE_MAX) {
                s_profile.overshoot_platea[range] = ema_blend(
                    s_profile.overshoot_platea[range], overshoot, s_profile.overshoot_sessions[range]);
                if (s_profile.overshoot_sessions[range] < 1000) {
                    s_profile.overshoot_sessions[range]++;
                }
            }

            s_tracking_overshoot_platea = false;
            s_overshoot_done_platea = true;
            s_profile_dirty = true;
            Serial.printf("ThermalProfile: platea overshoot=%.1f, appreso=%.1f (range %d)\n",
                           overshoot, s_profile.overshoot_platea[range], range);
        }
    }

    // 3b. Salita completata: inizia la saturazione della pietra e si impara
    //     di quanto la previsione iniziale ha sbagliato
    if (g_state.cielo_in_maintain && g_state.platea_in_maintain &&
        g_state.soak_start_ms == 0 && !g_state.stone_ready) {
        g_state.soak_start_ms = now;
        soak_cycle_reset();

        if (s_session_valid && !g_state.service_mode &&
            s_climb_start_ms > 0 && s_climb_predicted_min >= 3.0f) {
            float real = (float)(now - s_climb_start_ms) / 60000.0f;
            float bias = real / s_climb_predicted_min;

            if (bias >= ETA_BIAS_MIN && bias <= ETA_BIAS_MAX) {
                int range = get_target_range(g_state.target_platea);
                s_profile.eta_bias[range] = ema_blend_soft(
                    s_profile.eta_bias[range], bias, s_profile.eta_bias_sessions[range]);
                if (s_profile.eta_bias_sessions[range] < 1000) {
                    s_profile.eta_bias_sessions[range]++;
                }
                s_profile_dirty = true;
                Serial.printf("ThermalProfile: salita prevista %.1f min, reale %.1f -> bias %.2f (range %d)\n",
                               s_climb_predicted_min, real, s_profile.eta_bias[range], range);
            }
        }
    }

    // 4. Maintain autotune (MAINTAIN o READY, non COOKING)
    if (g_state.mode == OvenMode::MAINTAIN || g_state.mode == OvenMode::READY) {
        // Inizializza maintain corrente dal target range se appena entrati
        // Il ritorno da un boost non e' un ingresso nuovo: il mantenimento
        // tarato finora resta valido
        if (s_prev_mode != OvenMode::MAINTAIN && s_prev_mode != OvenMode::READY &&
            s_prev_mode != OvenMode::BOOST) {
            int range_c = get_target_range(g_state.target_cielo);
            int range_p = get_target_range(g_state.target_platea);
            s_current_maintain_cielo = s_profile.maintain_cielo[range_c];
            s_current_maintain_platea = s_profile.maintain_platea[range_p];
            s_last_maintain_check_ms = now;
            s_last_maintain_save_ms = now;
            s_maintain_temp_sum_cielo = 0;
            s_maintain_temp_sum_platea = 0;
            s_maintain_temp_count = 0;
            s_stable_start_cielo = 0;
            s_stable_start_platea = 0;
            s_back_to_target_cielo = false;
            s_back_to_target_platea = false;

            // Forno arrivato a regime senza passare da PREHEAT (riaccensione
            // a caldo): la quota di soak va decisa adesso
            if (s_climb_start_ms == 0) {
                s_soak_fraction = soak_fraction_from(g_state.temp_platea, g_state.target_platea);
            }
        }

        // --- Saturazione della pietra ---
        // La sonda sta SOTTO la pietra: "a target" non significa "pronto".
        // La pietra e' carica quando la potenza necessaria a tenere il target
        // smette di calare, cioe' quando non assorbe piu' calore.
        if (g_state.soak_start_ms > 0 && !g_state.stone_ready) {
            bool on = oven_state_resistance_platea_on();
            unsigned long soak_elapsed = now - g_state.soak_start_ms;

            // Un buco nei campioni (boost: si esce da MAINTAIN) o un boost in
            // corso falsano il ciclo: si riparte dalla prossima accensione.
            // Il primo campione non e' un'accensione anche se la resistenza e'
            // accesa: il ciclo in corso e' gia' iniziato, e con l'overshoot
            // della salita dentro darebbe un duty basso che non vuol dire nulla
            bool first = s_duty_last_ms == 0 || now - s_duty_last_ms > 10000;
            if (first || g_state.boost_platea) {
                s_duty_in_cycle = false;
                s_duty_prev_cycle = -1.0f;
                s_duty_settle_cycles = 0;
                s_duty_prev_on = on;
            }
            s_duty_last_ms = now;

            if (on && !s_duty_prev_on) {
                // Accensione: chiude il ciclo in corso e ne apre uno nuovo
                if (s_duty_in_cycle && s_duty_count >= 15) {
                    float duty = s_duty_sum_platea / (float)s_duty_count;
                    float period_min = (float)(now - s_duty_cycle_ms) / 60000.0f;
                    // Assestato = fermo in entrambe le direzioni: un duty che
                    // sale e' la pietra che assorbe ancora dopo l'overshoot
                    if (s_duty_prev_cycle >= 0 &&
                        fabsf(s_duty_prev_cycle - duty) < SOAK_DUTY_SETTLE) {
                        s_duty_settle_cycles++;
                    } else {
                        s_duty_settle_cycles = 0;
                    }
                    s_duty_prev_cycle = duty;
                    Serial.printf("ThermalProfile: soak ciclo %.1f min duty=%.0f%%, cicli stabili=%d\n",
                                   period_min, duty, s_duty_settle_cycles);
                }
                s_duty_in_cycle = true;
                s_duty_cycle_ms = now;
                s_duty_sum_platea = 0;
                s_duty_count = 0;
            }
            s_duty_prev_on = on;

            if (s_duty_in_cycle) {
                s_duty_sum_platea += on ? (float)oven_state_get_effective_power_platea() : 0.0f;
                s_duty_count++;
            }

            bool settled = (s_duty_settle_cycles >= SOAK_SETTLE_CYCLES) &&
                           (soak_elapsed >= (unsigned long)SOAK_MIN_MINUTES * 60000UL);
            bool timeout = soak_elapsed >= (unsigned long)SOAK_MAX_MINUTES * 60000UL;

            if (settled || timeout) {
                g_state.stone_ready = true;
                float measured = (float)soak_elapsed / 60000.0f;
                Serial.printf("ThermalProfile: pietra carica dopo %.1f min (%s)\n",
                               measured, timeout ? "timeout" : "duty assestato");

                // Impara solo da misure pulite e da salite sostanziali: su una
                // pietra gia' calda la misura non e' rappresentativa
                if (settled && s_session_valid && !g_state.service_mode &&
                    s_soak_fraction >= 0.5f) {
                    float full = measured / s_soak_fraction;  // normalizzato a pietra fredda
                    if (full >= (float)SOAK_MIN_MINUTES && full <= (float)SOAK_MAX_MINUTES) {
                        int range = get_target_range(g_state.target_platea);
                        s_profile.soak_minutes[range] = ema_blend_soft(
                            s_profile.soak_minutes[range], full, s_profile.soak_sessions[range]);
                        if (s_profile.soak_sessions[range] < 1000) {
                            s_profile.soak_sessions[range]++;
                        }
                        s_profile_dirty = true;
                        Serial.printf("ThermalProfile: soak appreso %.1f min (range %d)\n",
                                       s_profile.soak_minutes[range], range);
                    }
                }
            }
        }

        // Accumula temperature
        s_maintain_temp_sum_cielo += tc;
        s_maintain_temp_sum_platea += tp;
        s_maintain_temp_count++;

        // L'elemento e' rientrato: da qui in poi l'errore medio dipende davvero
        // dalla potenza di mantenimento e l'autotune puo' lavorarci
        // Vale solo vicino al target: sotto (target appena alzato) l'errore
        // dipende dalla salita, non dalla potenza di mantenimento
        if (fabsf(tc - (float)g_state.target_cielo) <= 2.0f) s_back_to_target_cielo = true;
        if (fabsf(tp - (float)g_state.target_platea) <= 2.0f) s_back_to_target_platea = true;

        // Ogni 60s: valuta e regola
        if (now - s_last_maintain_check_ms >= 60000 && s_maintain_temp_count >= 15) {
            s_last_maintain_check_ms = now;

            float avg_c = s_maintain_temp_sum_cielo / s_maintain_temp_count;
            float avg_p = s_maintain_temp_sum_platea / s_maintain_temp_count;
            float err_c = avg_c - (float)g_state.target_cielo;
            float err_p = avg_p - (float)g_state.target_platea;

            // Reset accumulatori
            s_maintain_temp_sum_cielo = 0;
            s_maintain_temp_sum_platea = 0;
            s_maintain_temp_count = 0;

            // Ignora se porta aperta (grande discrepanza). In servizio le
            // infornate ravvicinate falsano le medie: l'autotune verrebbe
            // spinto a gonfiare il mantenimento, sporcando il profilo appreso
            bool valid_c = (fabsf(err_c) < 15.0f) && !g_state.service_mode &&
                           s_back_to_target_cielo;
            bool valid_p = (fabsf(err_p) < 15.0f) && !g_state.service_mode &&
                           s_back_to_target_platea;

            if (valid_c) {
                if (err_c > 3.0f) {
                    s_current_maintain_cielo -= 2;
                    s_stable_start_cielo = 0;
                } else if (err_c < -3.0f) {
                    s_current_maintain_cielo += 2;
                    s_stable_start_cielo = 0;
                } else {
                    // Stabile
                    if (s_stable_start_cielo == 0) s_stable_start_cielo = now;
                }

                // Clamp
                if (s_current_maintain_cielo < MAINTAIN_MIN) s_current_maintain_cielo = MAINTAIN_MIN;
                if (s_current_maintain_cielo > MAINTAIN_MAX) s_current_maintain_cielo = MAINTAIN_MAX;
            }

            if (valid_p) {
                if (err_p > 3.0f) {
                    s_current_maintain_platea -= 2;
                    s_stable_start_platea = 0;
                } else if (err_p < -3.0f) {
                    s_current_maintain_platea += 2;
                    s_stable_start_platea = 0;
                } else {
                    if (s_stable_start_platea == 0) s_stable_start_platea = now;
                }

                if (s_current_maintain_platea < MAINTAIN_MIN) s_current_maintain_platea = MAINTAIN_MIN;
                if (s_current_maintain_platea > MAINTAIN_MAX) s_current_maintain_platea = MAINTAIN_MAX;
            }

            Serial.printf("ThermalProfile: maintain tune C=%d%% (err=%.1f) P=%d%% (err=%.1f)\n",
                           s_current_maintain_cielo, err_c,
                           s_current_maintain_platea, err_p);

            s_profile_dirty = true;

            // Se stabile per 2+ min: salva come appreso
            if (valid_c && s_stable_start_cielo > 0 && now - s_stable_start_cielo >= 120000) {
                int range = get_target_range(g_state.target_cielo);
                s_profile.maintain_cielo[range] = (uint8_t)s_current_maintain_cielo;
                if (s_profile.maintain_sessions[range] < 1000) {
                    s_profile.maintain_sessions[range]++;
                }
                s_profile_dirty = true;
                s_stable_start_cielo = now;  // Reset per non salvare ogni tick
                Serial.printf("ThermalProfile: cielo maintain convergato -> %d%% (range %d)\n",
                               s_current_maintain_cielo, range);
            }

            if (valid_p && s_stable_start_platea > 0 && now - s_stable_start_platea >= 120000) {
                int range = get_target_range(g_state.target_platea);
                s_profile.maintain_platea[range] = (uint8_t)s_current_maintain_platea;
                if (s_profile.maintain_sessions[range] < 1000) {
                    s_profile.maintain_sessions[range]++;
                }
                s_profile_dirty = true;
                s_stable_start_platea = now;
                Serial.printf("ThermalProfile: platea maintain convergato -> %d%% (range %d)\n",
                               s_current_maintain_platea, range);
            }
        }

        // Ogni 5 min: safety save
        if (s_profile_dirty && now - s_last_maintain_save_ms >= 300000) {
            s_last_maintain_save_ms = now;
            thermal_profile_save();
            Serial.println("ThermalProfile: safety save (5min)");
        }
    }

    // Update previous state
    s_prev_mode = g_state.mode;
    s_prev_cielo_in_maintain = g_state.cielo_in_maintain;
    s_prev_platea_in_maintain = g_state.platea_in_maintain;
}

// ============================================
// Real-time rate (from ring buffer)
// ============================================
float thermal_profile_get_realtime_rate_cielo() {
    if (s_history_count < 4) return 0;
    int oldest = (s_history_idx - s_history_count + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
    int newest = (s_history_idx - 1 + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
    float delta = s_temp_history_cielo[newest] - s_temp_history_cielo[oldest];
    float dt_min = (float)(s_time_history[newest] - s_time_history[oldest]) / 60000.0f;
    if (dt_min < 0.01f) return 0;
    return delta / dt_min;
}

float thermal_profile_get_realtime_rate_platea() {
    if (s_history_count < 4) return 0;
    int oldest = (s_history_idx - s_history_count + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
    int newest = (s_history_idx - 1 + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
    float delta = s_temp_history_platea[newest] - s_temp_history_platea[oldest];
    float dt_min = (float)(s_time_history[newest] - s_time_history[oldest]) / 60000.0f;
    if (dt_min < 0.01f) return 0;
    return delta / dt_min;
}

// ============================================
// ETA (float interna, usata da ETA pubblica e preheat power)
// ============================================
// Modello esponenziale: un forno non sale in modo lineare, il rate cala man
// mano che ci si avvicina all'equilibrio -> dT/dt = (T_inf - T) / tau.
// I due parametri si ricavano dai rate gia' appresi per banda: la relazione
// rate = T_inf/tau - T/tau e' una retta, quindi basta una regressione sui
// punti (temperatura media della banda, rate appreso). Piu' bande il forno
// impara, piu' il modello diventa suo. Servono almeno 2 bande con dati veri.
static const float BAND_MID[TEMP_BANDS] = {62.5f, 150.0f, 250.0f, 350.0f, 450.0f};

static bool fit_newton(bool is_cielo, float* tau, float* t_inf) {
    float sx = 0, sy = 0, sxx = 0, sxy = 0;
    int n = 0;

    // La banda piu' bassa viene esclusa: da freddo il forno e' in transitorio
    // (pietra e acciaio assorbono) e sale piu' lentamente che a 150 C, cioe'
    // in controtendenza rispetto al modello. Includerla appiattisce la retta e
    // porta a un T_inf assurdo, facendo scartare il fit.
    for (int b = 1; b < TEMP_BANDS; b++) {
        if (s_profile.rate_sessions[b] == 0) continue;  // solo bande imparate davvero
        float y = is_cielo ? s_profile.rate_cielo[b] : s_profile.rate_platea[b];
        if (y < 0.3f) continue;

        // Ascissa: la temperatura a cui il rate e' stato misurato davvero.
        // Il centro banda resta solo come ripiego finche' non c'e' la misura.
        float x = is_cielo ? s_profile.rate_temp_cielo[b] : s_profile.rate_temp_platea[b];
        if (x < 1.0f) x = BAND_MID[b];
        sx += x; sy += y; sxx += x * x; sxy += x * y; n++;
    }
    if (n < 2) return false;

    float den = (float)n * sxx - sx * sx;
    if (fabsf(den) < 1e-6f) return false;

    float slope = ((float)n * sxy - sx * sy) / den;   // -1/tau
    if (slope >= -1e-4f) return false;                // rate non decrescente: modello non valido

    float inter = (sy - slope * sx) / (float)n;       // T_inf/tau
    float t = -1.0f / slope;
    float ti = inter * t;

    if (t < MODEL_TAU_MIN || t > MODEL_TAU_MAX) return false;
    if (ti > MODEL_TINF_MAX) return false;

    *tau = t;
    *t_inf = ti;
    return true;
}

// ============================================
// ETA (float interna, usata da ETA pubblica e preheat power)
// ============================================
static float calc_eta_float(int temp, int target, bool is_cielo) {
    if (temp >= target) return 0.0f;

    // La velocita' misurata serve a correggere il modello, ma nei primi
    // secondi di accensione le resistenze si stanno ancora scaldando: il forno
    // sale di 2 C/min invece di 17 e la correzione gonfia l'ETA (misurato:
    // 21 min all'accensione, 27 trenta secondi dopo). Si aspetta di avere il
    // buffer pieno, cioe' 64s di misure vere.
    // In preriscaldamento, se l'elemento e' stato rallentato per aspettare
    // l'altro, la sua velocita' misurata e' bassa per scelta: correggerci
    // l'ETA la gonfierebbe, e con l'ETA gonfiata il bilanciamento gli
    // ridarebbe potenza, facendo oscillare la potenza a ogni lettura.
    float rt = 0.0f;
    bool rt_valid = g_state.mode != OvenMode::PREHEAT ||
                    window_full_power(is_cielo, RATE_HISTORY_SIZE);
    if (s_history_count >= RATE_HISTORY_SIZE && rt_valid) {
        rt = is_cielo ? thermal_profile_get_realtime_rate_cielo()
                      : thermal_profile_get_realtime_rate_platea();
    }

    // --- Modello esponenziale (preferito quando il forno ha imparato) ---
    float tau, t_inf;
    if (fit_newton(is_cielo, &tau, &t_inf) && (float)target < t_inf - 5.0f) {
        float eta = tau * logf((t_inf - (float)temp) / (t_inf - (float)target));

        // Correzione con il rate misurato ora: rapporto tra rate reale e rate
        // previsto dal modello alla temperatura corrente, entro limiti stretti
        // perche' la misura su 64s resta rumorosa
        float pred = (t_inf - (float)temp) / tau;
        if (rt > 0.3f && pred > 0.1f) {
            float k = rt / pred;
            if (k < 0.6f) k = 0.6f;
            if (k > 1.5f) k = 1.5f;
            eta /= k;
        }
        if (eta > 0.0f && eta < 600.0f) return eta;
    }

    // --- Fallback: modello lineare per bande (usato finche' non ha imparato) ---
    float eta = 0;
    int current_band = get_band(temp);
    int target_band = get_band(target);

    for (int b = current_band; b <= target_band; b++) {
        int band_start = (b == 0) ? 0 : BAND_LIMITS[b - 1];
        int band_end = BAND_LIMITS[b];

        int seg_start = (temp > band_start) ? temp : band_start;
        int seg_end = (target < band_end) ? target : band_end;
        int degrees = seg_end - seg_start;
        if (degrees <= 0) continue;

        float learned = is_cielo ? s_profile.rate_cielo[b] : s_profile.rate_platea[b];
        if (learned < 0.5f) {
            learned = is_cielo ? DEFAULT_RATE_CIELO : DEFAULT_RATE_PLATEA;
        }

        float rate = learned;

        // Rate real-time accettato solo dentro una finestra attorno a quello
        // appreso: senza limite superiore un picco di rumore faceva crollare
        // l'ETA a valori ottimistici
        if (b == current_band && rt >= learned * 0.5f && rt <= learned * 1.5f) {
            rate = rt;
        }

        eta += (float)degrees / rate;
    }

    return eta;
}

int thermal_profile_get_eta_minutes(int temp, int target, bool is_cielo) {
    float eta = calc_eta_float(temp, target, is_cielo);
    int result = (int)eta;
    if (result < 1 && temp < target) result = 1;
    return result;
}

// ============================================
// Soak: saturazione della pietra refrattaria
// ============================================

// Quota di soak ancora necessaria. Se la pietra e' gia' calda al momento di
// partire (riaccensione a caldo, target alzato di poco) la saturazione
// richiede molto meno tempo: contare sempre il soak pieno renderebbe l'ETA
// peggiore di una stima senza soak.
static float soak_fraction_from(int start_platea, int target_platea) {
    float span = (float)target_platea - 25.0f;
    if (span < 1.0f) return 0.0f;

    float f = (float)(target_platea - start_platea) / span;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f;
}

int thermal_profile_get_soak_total_minutes(int target) {
    int override_min = settings_get_soak_override();
    if (override_min > 0) return override_min;  // valore forzato dall'utente

    int range = get_target_range(target);
    int val = (int)(s_profile.soak_minutes[range] + 0.5f);
    if (val < SOAK_MIN_MINUTES) val = SOAK_MIN_MINUTES;
    if (val > SOAK_MAX_MINUTES) val = SOAK_MAX_MINUTES;
    return val;
}

// ============================================
// ETA al forno realmente pronto (salita + soak)
// ============================================
// Previsione grezza (senza bias) della salita delle sonde fino al cutoff,
// cioe' fino al punto in cui scatta il passaggio a mantenimento.
// Senza bias perche' e' il riferimento su cui il bias stesso viene appreso.
static float calc_climb_raw() {
    float climb = 0;

    if (!g_state.cielo_in_maintain && !g_state.error_cielo) {
        int cut = thermal_profile_get_cutoff_cielo(g_state.target_cielo);
        float e = calc_eta_float(g_state.temp_cielo, g_state.target_cielo - cut, true);
        if (e > climb) climb = e;
    }
    if (!g_state.platea_in_maintain && !g_state.error_platea) {
        int cut = thermal_profile_get_cutoff_platea(g_state.target_platea);
        float e = calc_eta_float(g_state.temp_platea, g_state.target_platea - cut, false);
        if (e > climb) climb = e;
    }
    return climb;
}

int thermal_profile_get_ready_eta_minutes() {
    if (!g_state.power_on || g_state.mode == OvenMode::STANDBY) {
        s_eta_displayed = -1;
        return 0;
    }
    if (g_state.stone_ready) {
        s_eta_displayed = 0;
        return 0;
    }
    // Pietra di nuovo da caricare (target alzato): lo "0" di prima non vale
    // piu', altrimenti un ETA di 1-3 min non lo supera mai
    if (s_eta_displayed == 0) s_eta_displayed = -1;

    // Correzione appresa: quanto questo forno sbaglia le proprie previsioni
    int range = get_target_range(g_state.target_platea);
    float climb = calc_climb_raw() * s_profile.eta_bias[range];

    // --- Soak residuo ---
    float soak_left = (float)thermal_profile_get_soak_total_minutes(g_state.target_platea)
                      * s_soak_fraction;
    if (g_state.soak_start_ms > 0) {
        soak_left -= (float)(millis() - g_state.soak_start_ms) / 60000.0f;
        if (soak_left < 0) soak_left = 0;
    }

    int total = (int)(climb + soak_left + 0.5f);
    if (total < 1) total = 1;  // finche' la pietra non e' carica non dire "0"

    // --- Smorzamento: un numero che oscilla sembra rotto anche se la media
    //     e' giusta. Aggiorna al massimo ogni 10s, scende liberamente, risale
    //     solo per scostamenti veri (porta aperta, target alzato) ---
    unsigned long now = millis();
    if (s_eta_displayed < 0) {
        s_eta_displayed = total;
        s_eta_last_update_ms = now;
    } else if (now - s_eta_last_update_ms >= 10000) {
        s_eta_last_update_ms = now;
        if (total < s_eta_displayed || total > s_eta_displayed + 3) {
            s_eta_displayed = total;
        }
    }
    return s_eta_displayed;
}

// ============================================
// Calo rapido del cielo (rilevamento infornata/porta aperta)
// ============================================
// Previsione fatta all'inizio della salita e tempo trascorso da allora:
// permette di confrontare previsto e reale dall'esterno, mentre succede.
float thermal_profile_get_climb_predicted_min() {
    return (s_climb_start_ms > 0) ? s_climb_predicted_min : 0.0f;
}

int thermal_profile_get_climb_elapsed_sec() {
    if (s_climb_start_ms == 0) return 0;
    return (int)((millis() - s_climb_start_ms) / 1000UL);
}

float thermal_profile_get_drop_cielo_20s() {
    if (s_history_count < 11) return 0.0f;

    int newest = (s_history_idx - 1 + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;
    int older  = (s_history_idx - 11 + RATE_HISTORY_SIZE) % RATE_HISTORY_SIZE;  // ~20s
    float dt_s = (float)(s_time_history[newest] - s_time_history[older]) / 1000.0f;
    if (dt_s < 1.0f) return 0.0f;
    // Riportato a esattamente 20s: la soglia INFORNATA_DROP_C e' tarata su quelli
    return (s_temp_history_cielo[older] - s_temp_history_cielo[newest]) * (20.0f / dt_s);
}

// Minuti per riportare un elemento al suo target dopo un'infornata.
// Usa lo stesso modello dell'ETA di salita, quindi migliora con l'uso.
int thermal_profile_get_target_eta_minutes(bool is_cielo) {
    int temp   = is_cielo ? g_state.temp_cielo : g_state.temp_platea;
    int target = is_cielo ? g_state.target_cielo : g_state.target_platea;
    if (temp >= target) return 0;

    float eta = calc_eta_float(temp, target, is_cielo);
    int m = (int)(eta + 0.5f);
    return (m < 1) ? 1 : m;
}

// ============================================
// Serializzazione JSON del profilo appreso
// ============================================
size_t thermal_profile_to_json(char* buf, size_t len) {
    size_t pos = 0;
    if (len == 0) return 0;
    // snprintf restituisce quanto avrebbe scritto: senza il controllo, a buffer
    // pieno len - pos va in underflow e la scrittura successiva esce dal buffer
#define JSON_APPEND(...) do { \
        int n_ = snprintf(buf + pos, len - pos, __VA_ARGS__); \
        if (n_ < 0 || (size_t)n_ >= len - pos) { buf[len - 1] = '\0'; return len - 1; } \
        pos += (size_t)n_; \
    } while (0)
    float tau_c = 0, tinf_c = 0, tau_p = 0, tinf_p = 0;
    bool fit_c = fit_newton(true, &tau_c, &tinf_c);
    bool fit_p = fit_newton(false, &tau_p, &tinf_p);

    JSON_APPEND("{\"ver\":%d,\"bands\":[", s_profile.version);
    for (int b = 0; b < TEMP_BANDS; b++) {
        JSON_APPEND(
            "%s{\"rc\":%.2f,\"rp\":%.2f,\"tmc\":%.0f,\"tmp\":%.0f,\"n\":%u}",
            b ? "," : "", s_profile.rate_cielo[b], s_profile.rate_platea[b],
            s_profile.rate_temp_cielo[b], s_profile.rate_temp_platea[b],
            (unsigned)s_profile.rate_sessions[b]);
    }

    JSON_APPEND("],\"ranges\":[");
    for (int r = 0; r < TARGET_RANGES; r++) {
        JSON_APPEND(
            "%s{\"ovc\":%.1f,\"ovp\":%.1f,\"ovn\":%u,"
            "\"mc\":%u,\"mp\":%u,\"mn\":%u,"
            "\"soak\":%.1f,\"soakn\":%u,"
            "\"bias\":%.2f,\"biasn\":%u}",
            r ? "," : "",
            s_profile.overshoot_cielo[r], s_profile.overshoot_platea[r],
            (unsigned)s_profile.overshoot_sessions[r],
            (unsigned)s_profile.maintain_cielo[r], (unsigned)s_profile.maintain_platea[r],
            (unsigned)s_profile.maintain_sessions[r],
            s_profile.soak_minutes[r], (unsigned)s_profile.soak_sessions[r],
            s_profile.eta_bias[r], (unsigned)s_profile.eta_bias_sessions[r]);
    }

    // Modello esponenziale stimato dai rate appresi (vuoto = non ancora valido)
    JSON_APPEND(
        "],\"model\":{\"okc\":%s,\"tauc\":%.1f,\"tinfc\":%.0f,"
        "\"okp\":%s,\"taup\":%.1f,\"tinfp\":%.0f},",
        fit_c ? "true" : "false", tau_c, tinf_c,
        fit_p ? "true" : "false", tau_p, tinf_p);

    // Stato corrente della sessione
    JSON_APPEND(
        "\"live\":{\"maintc\":%d,\"maintp\":%d,"
        "\"soakfrac\":%.2f,\"rtc\":%.1f,\"rtp\":%.1f,\"valid\":%s,"
        "\"pred\":%.1f,\"clb\":%d}}",
        s_current_maintain_cielo, s_current_maintain_platea,
        s_soak_fraction,
        thermal_profile_get_realtime_rate_cielo(),
        thermal_profile_get_realtime_rate_platea(),
        s_session_valid ? "true" : "false",
        thermal_profile_get_climb_predicted_min(),
        thermal_profile_get_climb_elapsed_sec());

#undef JSON_APPEND
    return pos;
}

void thermal_profile_invalidate_session() {
    s_session_valid = false;
    soak_cycle_reset();
}

// ============================================
// Cutoff (gradi prima del target per switch a maintain)
// ============================================
int thermal_profile_get_cutoff_cielo(int target) {
    int range = get_target_range(target);
    if (s_profile.overshoot_sessions[range] > 0) {
        int cutoff = (int)(s_profile.overshoot_cielo[range] + 0.5f);
        if (cutoff < 3) cutoff = 3;
        if (cutoff > 30) cutoff = 30;
        return cutoff;
    }
    return (int)(DEFAULT_OVERSHOOT_CIELO + 0.5f);  // 4
}

int thermal_profile_get_cutoff_platea(int target) {
    int range = get_target_range(target);
    if (s_profile.overshoot_sessions[range] > 0) {
        int cutoff = (int)(s_profile.overshoot_platea[range] + 0.5f);
        if (cutoff < 3) cutoff = 3;
        if (cutoff > 30) cutoff = 30;
        return cutoff;
    }
    return (int)(DEFAULT_OVERSHOOT_PLATEA + 0.5f);  // 10
}

// ============================================
// Maintain power
// ============================================
uint8_t thermal_profile_get_maintain_power_cielo(int target) {
    // Se autotune attivo (in MAINTAIN/READY), usa valore runtime
    if (g_state.mode == OvenMode::MAINTAIN || g_state.mode == OvenMode::READY) {
        return (uint8_t)s_current_maintain_cielo;
    }
    // Se in PREHEAT con cielo in maintain, usa valore runtime
    if (g_state.mode == OvenMode::PREHEAT && g_state.cielo_in_maintain) {
        return (uint8_t)s_current_maintain_cielo;
    }
    // Altrimenti: valore dal profilo per il target range
    int range = get_target_range(target);
    return s_profile.maintain_cielo[range];
}

uint8_t thermal_profile_get_maintain_power_platea(int target) {
    if (g_state.mode == OvenMode::MAINTAIN || g_state.mode == OvenMode::READY) {
        return (uint8_t)s_current_maintain_platea;
    }
    if (g_state.mode == OvenMode::PREHEAT && g_state.platea_in_maintain) {
        return (uint8_t)s_current_maintain_platea;
    }
    int range = get_target_range(target);
    return s_profile.maintain_platea[range];
}

// ============================================
// Preheat power proporzionale
// ============================================

// Limita la variazione della potenza di preriscaldamento nel tempo. Dopo una
// pausa (preriscaldamento appena iniziato) riparte dal valore calcolato.
static int preheat_slew(int target_pw, bool is_cielo) {
    static float s_pw[2] = {100.0f, 100.0f};
    static unsigned long s_last_ms[2] = {0, 0};
    int i = is_cielo ? 0 : 1;
    unsigned long now = millis();
    float dt_s = (float)(now - s_last_ms[i]) / 1000.0f;
    s_last_ms[i] = now;

    if (dt_s > 10.0f) {
        s_pw[i] = (float)target_pw;
    } else {
        float step = PREHEAT_POWER_SLEW * dt_s;
        float diff = (float)target_pw - s_pw[i];
        if (diff > step) diff = step;
        if (diff < -step) diff = -step;
        s_pw[i] += diff;
    }
    return (int)(s_pw[i] + 0.5f);
}

void thermal_profile_get_preheat_power(
    int temp_cielo, int target_cielo,
    int temp_platea, int target_platea,
    int* preheat_cielo, int* preheat_platea)
{
    float eta_c = calc_eta_float(temp_cielo, target_cielo, true);
    float eta_p = calc_eta_float(temp_platea, target_platea, false);

    // Entrambi quasi a target: 100% / 100%
    if (eta_c < 1.0f && eta_p < 1.0f) {
        *preheat_cielo = preheat_slew(100, true);
        *preheat_platea = preheat_slew(100, false);
        return;
    }

    int pw_c, pw_p;

    if (eta_c >= eta_p) {
        // Cielo piu' lento: cielo 100%, platea ridotta
        pw_c = 100;
        pw_p = (eta_c > 0.01f) ? (int)(100.0f * eta_p / eta_c + 0.5f) : 100;
    } else {
        // Platea piu' lenta: platea 100%, cielo ridotta
        pw_p = 100;
        pw_c = (eta_p > 0.01f) ? (int)(100.0f * eta_c / eta_p + 0.5f) : 100;
    }

    // Clamp a [60, 100]
    if (pw_c < 60) pw_c = 60;
    if (pw_c > 100) pw_c = 100;
    if (pw_p < 60) pw_p = 60;
    if (pw_p > 100) pw_p = 100;

    pw_c = preheat_slew(pw_c, true);
    pw_p = preheat_slew(pw_p, false);

    *preheat_cielo = pw_c;
    *preheat_platea = pw_p;

    static unsigned long s_last_log_ms = 0;
    unsigned long now = millis();
    if (now - s_last_log_ms >= 10000) {
        s_last_log_ms = now;
        Serial.printf("PreheatPower: ETA c=%.1f p=%.1f -> pw c=%d%% p=%d%%\n",
                       eta_c, eta_p, pw_c, pw_p);
    }
}
