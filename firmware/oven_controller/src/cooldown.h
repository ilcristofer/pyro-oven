/**
 * Cooldown - Stima del tempo di raffreddamento a forno spento
 *
 * Con le resistenze spente il forno perde calore per convezione (~ T - Ta)
 * e per irraggiamento, che pesa sempre meno man mano che si raffredda: la
 * costante di tempo non e' fissa ma cresce scendendo,
 * tau(T) = C0 / (1 + K * (T - Ta)) (sessioni 16/9, 13/10, 14/10: ~50 min
 * a 300 °C, ~140 a 45 °C sul cielo). Il tempo da T1 a T2 ha forma chiusa:
 * t = C0 * ln(x1 * (1 + K*x2) / (x2 * (1 + K*x1))), con x = T - Ta.
 * A ogni spegnimento si impara un fattore di scala su C0. Ta e' il giunto
 * freddo dei MAX31855, nella scatola del controller.
 */

#ifndef COOLDOWN_H
#define COOLDOWN_H

#include <Arduino.h>

#define COOL_TARGET_C           40      // °C: sotto questa il forno e' "freddo"

// Forma di tau(T) misurata (discese 2026-09-16, 2026-10-13, 2026-10-14).
// Verificata sulla notte 14-15/10 fino a 33 °C con Ta = scatola (27-28 °C):
// scala 0.95 cielo, 0.98 platea, scarto medio 0.4-0.7 °C
#define COOL_C0_CIELO           153.0f  // min, tau vicino all'ambiente
#define COOL_K_CIELO            0.0076f // 1/°C
#define COOL_C0_PLATEA          128.0f  // min (sonda sotto la pietra)
#define COOL_K_PLATEA           0.0038f
#define COOL_DEFAULT_AMBIENT    25.0f   // °C, finche' i MAX31855 non rispondono

#define COOL_SCALE_MIN          0.5f    // limiti di plausibilita' del fattore appreso
#define COOL_SCALE_MAX          2.5f

void  cooldown_init();

// Da chiamare ogni 2s dopo sensors_read()
void  cooldown_record();

// Minuti perche' il forno scenda sotto COOL_TARGET_C.
// 0 = gia' freddo o forno acceso, -1 = sonde in errore.
int   cooldown_get_minutes();

// Diagnostica: tau in uso alla temperatura attuale (min) e ambiente assunta
float cooldown_get_tau(bool is_cielo);
float cooldown_get_ambient();

void  cooldown_reset();

#endif // COOLDOWN_H
