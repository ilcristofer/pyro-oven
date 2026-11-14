/**
 * Cooldown - Implementazione
 *
 * A ogni spegnimento da caldo si fissa un punto di partenza (3 minuti dopo,
 * quando l'aria calda della camera se n'e' andata) e si confronta il tempo
 * reale con quello del modello sullo stesso calo: il rapporto e' il fattore
 * di scala della sessione. Siccome la forma di tau(T) e' gia' nel modello,
 * una discesa parziale (fermata riaccendendo) insegna lo stesso fattore di
 * una completa. Con il tau fisso della prima versione la discesa in corso,
 * misurata nella parte calda e veloce, accorciava la stima di un'ora e mezza
 * (14/10: da 366 a 270 minuti in un colpo, a 13 minuti dallo spegnimento).
 */

#include "cooldown.h"
#include "oven_state.h"
#include <Preferences.h>
#include <math.h>

#define COOL_NVS_NAMESPACE  "cool"

#define COOL_START_MIN_C        100     // sotto questa lo spegnimento non insegna nulla
#define COOL_ANCHOR_DELAY_MS    180000UL // 3 min: il primo calo e' l'aria, non la massa
#define COOL_FIT_INTERVAL_MS    60000UL
#define COOL_LIVE_MIN_MS        600000UL  // 10 min di discesa per una scala di sessione
#define COOL_LIVE_MIN_RATIO     1.1f
#define COOL_LEARN_MIN_MS       1200000UL // 20 min per impararla
#define COOL_LEARN_MIN_RATIO    1.3f

// Temperatura ambiente: giunto freddo dei MAX31855 nella scatola del
// controller. Fino alla prima versione era appresa dalle sonde a forno
// "fermo", ma a 35 gradi il forno scende ancora di 0,3 gradi in 5 minuti:
// il 14/10 ha imparato 34 con la stanza a ~27, e la stima e' risultata
// lunga di 70 minuti
#define COOL_AMB_MIN_C          5.0f
#define COOL_AMB_LIMIT_C        35.0f     // Ta non puo' avvicinarsi troppo al target

// Appreso: fattore di scala sulla forma misurata di tau(T)
static float s_scale_c = 1.0f;
static float s_scale_p = 1.0f;
static uint16_t s_n = 0;
static float s_ambient = COOL_DEFAULT_AMBIENT;

// Discesa in corso
static bool s_active = false;
static unsigned long s_start_ms = 0;
static unsigned long s_anchor_ms = 0;       // 0 = punto di partenza non ancora fissato
static int s_anchor_c = 0, s_anchor_p = 0;
static unsigned long s_last_fit_ms = 0;
static float s_sess_scale_c = 0, s_sess_scale_p = 0;    // 0 = non ancora misurato
static float s_sess_ratio_c = 0, s_sess_ratio_p = 0;

static float clamp_ambient(float a) {
    if (a < COOL_AMB_MIN_C) return COOL_AMB_MIN_C;
    if (a > COOL_AMB_LIMIT_C) return COOL_AMB_LIMIT_C;
    return a;
}

static bool scale_ok(float s) {
    return s >= COOL_SCALE_MIN && s <= COOL_SCALE_MAX;
}

// Minuti per scendere da t1 a t2 con il fattore di scala dato
// (0 = non calcolabile: t2 troppo vicina all'ambiente o non sotto t1)
static float model_minutes(bool cielo, float t1, float t2, float scale) {
    float c0 = cielo ? COOL_C0_CIELO : COOL_C0_PLATEA;
    float k = cielo ? COOL_K_CIELO : COOL_K_PLATEA;
    float x1 = t1 - s_ambient, x2 = t2 - s_ambient;
    if (x2 <= 0.5f || x1 <= x2) return 0;
    return scale * c0 * logf(x1 * (1.0f + k * x2) / (x2 * (1.0f + k * x1)));
}

static void save() {
    Preferences p;
    p.begin(COOL_NVS_NAMESPACE, false);
    p.putFloat("s2c", s_scale_c);
    p.putFloat("s2p", s_scale_p);
    p.putUShort("n2", s_n);
    p.end();
}

void cooldown_init() {
    Preferences p;
    p.begin(COOL_NVS_NAMESPACE, true);
    // Chiavi "s2": quanto appreso prima (tc/tp, sc/sp) era misurato contro
    // l'ambiente appreso sbagliato, si riparte dalla forma misurata
    s_scale_c = p.getFloat("s2c", 1.0f);
    s_scale_p = p.getFloat("s2p", 1.0f);
    s_n = p.getUShort("n2", 0);
    p.end();

    if (!scale_ok(s_scale_c)) s_scale_c = 1.0f;
    if (!scale_ok(s_scale_p)) s_scale_p = 1.0f;
    Serial.printf("Cooldown: scala cielo=%.2f platea=%.2f (n=%u)\n",
                  s_scale_c, s_scale_p, s_n);
}

// Fattore di scala dalla discesa tra il punto di partenza e adesso
// (0 = non misurabile)
static float fit_scale(bool cielo, int t0, int t, float dt_min, float min_ratio, float* ratio) {
    float a = (float)t0 - s_ambient;
    float b = (float)t - s_ambient;
    *ratio = 0;
    if (a <= 0 || b <= 0.5f) return 0;
    float r = a / b;
    if (r < min_ratio) return 0;
    float m = model_minutes(cielo, t0, t, 1.0f);
    if (m <= 0) return 0;
    float sc = dt_min / m;
    if (!scale_ok(sc)) return 0;
    *ratio = r;
    return sc;
}

static float learn(float old_scale, float measured) {
    // Il primo valore pesa meta' (la forma viene da quattro sole discese),
    // poi media mobile sempre piu' stabile ma mai ferma
    if (s_n == 0) return (old_scale + measured) * 0.5f;
    float w = 1.0f / (float)(s_n + 1);
    if (w < 0.2f) w = 0.2f;
    return old_scale + (measured - old_scale) * w;
}

static void end_session(unsigned long now) {
    if (s_anchor_ms > 0 && now - s_anchor_ms >= COOL_LEARN_MIN_MS) {
        bool learned = false;
        if (s_sess_scale_c > 0 && s_sess_ratio_c >= COOL_LEARN_MIN_RATIO) {
            s_scale_c = learn(s_scale_c, s_sess_scale_c);
            learned = true;
        }
        if (s_sess_scale_p > 0 && s_sess_ratio_p >= COOL_LEARN_MIN_RATIO) {
            s_scale_p = learn(s_scale_p, s_sess_scale_p);
            learned = true;
        }
        if (learned) {
            if (s_n < 1000) s_n++;
            save();
            Serial.printf("Cooldown: appreso scala cielo=%.2f platea=%.2f (n=%u)\n",
                          s_scale_c, s_scale_p, s_n);
        }
    }
    s_active = false;
    s_anchor_ms = 0;
    s_sess_scale_c = s_sess_scale_p = 0;
    s_sess_ratio_c = s_sess_ratio_p = 0;
}

void cooldown_record() {
    unsigned long now = millis();

    // Valido se almeno un MAX31855 risponde (sensors.cpp)
    if (!g_state.error_cielo || !g_state.error_platea) {
        s_ambient = clamp_ambient((float)g_state.temp_ambient);
    }

    if (g_state.power_on) {
        if (s_active) end_session(now);
        return;
    }

    bool ok_c = !g_state.error_cielo, ok_p = !g_state.error_platea;
    int hot = -1000;
    if (ok_c) hot = g_state.temp_cielo;
    if (ok_p && g_state.temp_platea > hot) hot = g_state.temp_platea;

    // Anche dopo un riavvio a forno caldo (es. aggiornamento) la discesa conta
    if (!s_active && hot >= COOL_START_MIN_C) {
        s_active = true;
        s_start_ms = now;
        s_anchor_ms = 0;
        s_last_fit_ms = now;
    }
    if (!s_active) return;

    if (s_anchor_ms == 0) {
        if (now - s_start_ms >= COOL_ANCHOR_DELAY_MS) {
            s_anchor_ms = now;
            s_anchor_c = g_state.temp_cielo;
            s_anchor_p = g_state.temp_platea;
        }
        return;
    }

    if (now - s_last_fit_ms >= COOL_FIT_INTERVAL_MS) {
        s_last_fit_ms = now;
        unsigned long el = now - s_anchor_ms;
        if (el >= COOL_LIVE_MIN_MS) {
            float dt = el / 60000.0f;
            if (ok_c) s_sess_scale_c = fit_scale(true, s_anchor_c, g_state.temp_cielo, dt,
                                                 COOL_LIVE_MIN_RATIO, &s_sess_ratio_c);
            if (ok_p) s_sess_scale_p = fit_scale(false, s_anchor_p, g_state.temp_platea, dt,
                                                 COOL_LIVE_MIN_RATIO, &s_sess_ratio_p);
        }
    }

    if (hot < COOL_TARGET_C) end_session(now);
}

static float scale_in_use(bool is_cielo) {
    float learned = is_cielo ? s_scale_c : s_scale_p;
    float sess = is_cielo ? s_sess_scale_c : s_sess_scale_p;
    // La discesa in corso dice com'e' andata oggi (porta aperta, stanza fredda),
    // l'appreso pesa tutte le altre
    return sess > 0 ? (learned + sess) * 0.5f : learned;
}

float cooldown_get_tau(bool is_cielo) {
    int t = is_cielo ? g_state.temp_cielo : g_state.temp_platea;
    float x = (float)t - s_ambient;
    if (x < 0) x = 0;
    float c0 = is_cielo ? COOL_C0_CIELO : COOL_C0_PLATEA;
    float k = is_cielo ? COOL_K_CIELO : COOL_K_PLATEA;
    return scale_in_use(is_cielo) * c0 / (1.0f + k * x);
}

float cooldown_get_ambient() {
    return s_ambient;
}

int cooldown_get_minutes() {
    if (g_state.power_on) return 0;
    if (g_state.error_cielo && g_state.error_platea) return -1;

    float worst = 0;
    for (int i = 0; i < 2; i++) {
        bool cielo = (i == 0);
        if (cielo ? g_state.error_cielo : g_state.error_platea) continue;
        int t = cielo ? g_state.temp_cielo : g_state.temp_platea;
        if (t <= COOL_TARGET_C) continue;
        float m = model_minutes(cielo, t, COOL_TARGET_C, scale_in_use(cielo));
        if (m > worst) worst = m;
    }
    return worst > 0 ? (int)ceilf(worst) : 0;
}

void cooldown_reset() {
    s_scale_c = 1.0f;
    s_scale_p = 1.0f;
    s_n = 0;
    Preferences p;
    p.begin(COOL_NVS_NAMESPACE, false);
    p.clear();
    p.end();
    s_active = false;
    s_anchor_ms = 0;
    s_sess_scale_c = s_sess_scale_p = 0;
}
