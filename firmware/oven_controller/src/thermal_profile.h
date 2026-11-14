/**
 * Thermal Profile - Auto-learning termico
 * Sostituisce il modulo di calibrazione manuale.
 * Impara dal funzionamento normale e aggiorna il profilo termico
 * automaticamente. I default iniziali sono basati su dati reali.
 */

#ifndef THERMAL_PROFILE_H
#define THERMAL_PROFILE_H

#include <Arduino.h>

// ============================================
// Costanti
// ============================================
#define TEMP_BANDS          5   // 25-100, 100-200, 200-300, 300-400, 400+
#define TARGET_RANGES       3   // <250, 250-350, >350
#define RATE_HISTORY_SIZE   32  // Ring buffer (32 campioni = 64s a 2s/lettura)
#define THERMAL_PROFILE_VER 6   // Versione NVS (v6: rate solo a piena potenza, vedi migrazione)

// Preriscaldamento: l'elemento che arriverebbe prima viene rallentato perche'
// arrivino insieme. La sua velocita' a potenza ridotta non e' quella del forno:
// si impara, e si usa per correggere l'ETA, solo sopra questa potenza.
#define PREHEAT_FULL_POWER_MIN  95      // %
// Variazione massima della potenza di preriscaldamento: senza limite salta di
// 5-10 punti a ogni lettura seguendo il rumore della velocita' misurata
#define PREHEAT_POWER_SLEW      0.5f    // %/s (30 punti al minuto, il bilanciamento ne chiede ~12)

// Default dal forno reale. Ricavati dal modello esponenziale misurato sulla
// sessione 2026-09-16: cielo tau=17.0min T_inf=442C, platea tau=8.6min
// T_inf=352C (la platea in preriscaldamento lavora a potenza ridotta per
// arrivare a target insieme al cielo, quindi il suo asintoto apparente e' piu'
// basso di quello a piena potenza).
#define DEFAULT_RATE_CIELO      7.9f    // C/min (medio, fallback)
#define DEFAULT_RATE_PLATEA     8.6f    // C/min (medio, fallback)
#define DEFAULT_OVERSHOOT_CIELO 4.0f    // C
#define DEFAULT_OVERSHOOT_PLATEA 10.0f  // C
#define DEFAULT_MAINTAIN        60      // %

// Limiti maintain
#define MAINTAIN_MIN            5       // %
#define MAINTAIN_MAX            90      // %

// Soak: tempo di saturazione della pietra refrattaria dopo che la sonda
// (montata SOTTO la pietra) ha raggiunto il target. Fino a fine soak la
// superficie di cottura e' piu' fredda del valore letto.
#define DEFAULT_SOAK_LOW        8       // min, target <250C
#define DEFAULT_SOAK_MID        10      // min, target 250-350C
#define DEFAULT_SOAK_HIGH       15      // min, target >350C
#define SOAK_MIN_MINUTES        2       // sotto questa soglia la misura e' scartata
#define SOAK_MAX_MINUTES        30      // oltre: pietra dichiarata carica comunque
#define SOAK_DUTY_SETTLE        3.0f    // punti % di variazione tra due cicli sotto cui il duty e' assestato
// Cicli interi del termostato (accensione -> accensione) consecutivi con il
// duty non piu' in calo. Un ciclo contiene sia la parte sotto sia quella sopra
// il target, quindi non serve scartare i tratti sopra target.
#define SOAK_SETTLE_CYCLES      1

// Conferma del picco di overshoot. Il criterio precedente (3 letture
// consecutive in calo) non scattava mai a bassa temperatura: il forno perde
// ~1 C ogni 40s e le letture intere si ripetono uguali, azzerando il conteggio.
#define OVERSHOOT_CONFIRM_DROP  2.0f    // C sotto il picco = picco confermato
#define OVERSHOOT_CONFIRM_MS    90000UL // oppure: nessun nuovo picco per 90s

// Campioni minimi perche' la media di una banda venga imparata (8 = ~16s)
#define RATE_MIN_SAMPLES        8

// Correzione ETA appresa (tempo reale / tempo previsto)
#define ETA_BIAS_MIN            0.5f
#define ETA_BIAS_MAX            2.0f

// Limiti di plausibilita' del modello esponenziale dT/dt = (T_inf - T) / tau
#define MODEL_TAU_MIN           10.0f   // min
#define MODEL_TAU_MAX           300.0f  // min
#define MODEL_TINF_MAX          800.0f  // C

// ============================================
// Struttura profilo persistente (NVS)
// ============================================
struct ThermalProfile {
    uint8_t version;

    // Rate per banda (C/min)
    float rate_cielo[TEMP_BANDS];
    float rate_platea[TEMP_BANDS];
    uint16_t rate_sessions[TEMP_BANDS];

    // Overshoot per target range
    float overshoot_cielo[TARGET_RANGES];
    float overshoot_platea[TARGET_RANGES];
    uint16_t overshoot_sessions[TARGET_RANGES];

    // Maintain per target range
    uint8_t maintain_cielo[TARGET_RANGES];
    uint8_t maintain_platea[TARGET_RANGES];
    uint16_t maintain_sessions[TARGET_RANGES];

    // --- v2: campi aggiunti in coda (i precedenti mantengono l'offset,
    //     cosi' un profilo v1 si puo' migrare senza perdere l'appreso) ---

    // Soak pietra (min) per target range
    float soak_minutes[TARGET_RANGES];
    uint16_t soak_sessions[TARGET_RANGES];

    // Correzione ETA appresa (reale/previsto) per target range
    float eta_bias[TARGET_RANGES];
    uint16_t eta_bias_sessions[TARGET_RANGES];

    // --- v3 ---
    // Temperatura media alla quale i rate di ciascuna banda sono stati davvero
    // misurati. Non coincide con il centro della banda: i campioni si
    // addensano dove il forno rallenta e dove si ferma il target, e attribuire
    // il rate al centro falsava la stima del modello (T_inf sovrastimato di
    // ~70 C sui dati reali). 0 = non ancora misurata, si usa il centro.
    float rate_temp_cielo[TEMP_BANDS];
    float rate_temp_platea[TEMP_BANDS];
};

// ============================================
// Funzioni pubbliche
// ============================================

// Inizializza: carica da NVS o usa default
void thermal_profile_init();

// Registra dati dal funzionamento (chiamare dopo sensors_read(), ogni 2s)
void thermal_profile_record();

// Salva profilo su NVS (chiamato anche internamente)
void thermal_profile_save();
void thermal_profile_save_if_dirty();   // scrive su NVS solo se ci sono dati nuovi

// Factory reset: cancella profilo, riparte dai default
void thermal_profile_reset();

// --- Query per ETA ---
float thermal_profile_get_realtime_rate_cielo();
float thermal_profile_get_realtime_rate_platea();
int   thermal_profile_get_eta_minutes(int temp, int target, bool is_cielo);

// Minuti mancanti al forno realmente pronto per infornare: salita delle sonde
// + saturazione della pietra. 0 = pronto. E' la stima da mostrare all'utente.
int   thermal_profile_get_ready_eta_minutes();

// Soak totale previsto per il target (override utente, appreso o default)
int   thermal_profile_get_soak_total_minutes(int target);

// Calo di temperatura del cielo negli ultimi ~20s (positivo = sta scendendo).
// La sonda cielo e' in camera: reagisce in secondi all'apertura porta e
// all'infornata, molto prima della sonda sotto la pietra.
float thermal_profile_get_drop_cielo_20s();

// Invalida gli apprendimenti della sessione corrente (soak e bias ETA):
// da chiamare quando un recupero/infornata falsa la misura in corso
void  thermal_profile_invalidate_session();

// Serializza in JSON il profilo appreso (diagnostica, endpoint /api/profile).
// Ritorna i byte scritti.
size_t thermal_profile_to_json(char* buf, size_t len);

// Minuti per riportare un elemento al suo target (recupero dopo l'infornata).
// 0 = gia' a target.
int   thermal_profile_get_target_eta_minutes(bool is_cielo);

// Diagnostica: previsione di salita fatta all'inizio del riscaldamento e
// secondi trascorsi da quel momento. Servono a confrontare previsto e reale
// dall'esterno (telemetria) senza doverlo ricostruire a posteriori.
// 0 = nessuna salita in corso.
float thermal_profile_get_climb_predicted_min();
int   thermal_profile_get_climb_elapsed_sec();

// --- Query per cutoff (gradi prima del target per switch a maintain) ---
int thermal_profile_get_cutoff_cielo(int target);
int thermal_profile_get_cutoff_platea(int target);

// --- Query per maintain power ---
uint8_t thermal_profile_get_maintain_power_cielo(int target);
uint8_t thermal_profile_get_maintain_power_platea(int target);

// --- Preheat power proporzionale (arrivano insieme al target) ---
// Calcola potenze preheat proporzionali per sincronizzare cielo e platea.
// Risultato in preheat_cielo e preheat_platea (60-100%).
void thermal_profile_get_preheat_power(
    int temp_cielo, int target_cielo,
    int temp_platea, int target_platea,
    int* preheat_cielo, int* preheat_platea);

#endif // THERMAL_PROFILE_H
