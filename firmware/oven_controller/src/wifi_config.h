/**
 * WiFi Configuration
 * Credenziali salvate in NVS tramite wifi_storage
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// ============================================
// Credenziali WiFi di fallback (usate se NVS vuoto)
// ============================================
// Stanno in wifi_secrets.h, fuori da git (modello: wifi_secrets.example.h).
// Senza quel file la rete si configura dal touch: Impostazioni > WiFi
#if __has_include("wifi_secrets.h")
#include "wifi_secrets.h"
#endif
#ifndef WIFI_SSID_DEFAULT
#define WIFI_SSID_DEFAULT     ""
#endif
#ifndef WIFI_PASSWORD_DEFAULT
#define WIFI_PASSWORD_DEFAULT ""
#endif

// ============================================
// NTP Configuration
// ============================================
#define NTP_SERVER    "pool.ntp.org"    // Server NTP
#define GMT_OFFSET    3600              // Offset GMT in secondi (Italia = +1 ora = 3600)
#define DST_OFFSET    3600              // Ora legale in secondi (3600 se attiva, 0 se no)
// Fuso POSIX con il cambio d'ora automatico (ultima domenica di marzo e di
// ottobre). GMT_OFFSET + DST_OFFSET fissi davano l'ora sbagliata d'inverno
#define TZ_INFO       "CET-1CEST,M3.5.0,M10.5.0/3"

// ============================================
// WiFi Options
// ============================================
#define WIFI_TIMEOUT_MS     10000       // Timeout connessione (10 sec)
#define NTP_SYNC_INTERVAL   3600000     // Risincronizza NTP ogni ora (ms)

#endif // WIFI_CONFIG_H
