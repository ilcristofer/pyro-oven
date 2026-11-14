/**
 * WiFi and NTP Time Management
 * Gestisce connessione WiFi e sincronizzazione ora via NTP
 */

#ifndef WIFI_TIME_H
#define WIFI_TIME_H

#include <Arduino.h>

// ============================================
// WiFi Status
// ============================================
enum class WiFiStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    FAILED
};

// ============================================
// Functions
// ============================================

/**
 * Inizializza WiFi e NTP
 * Tenta la connessione in modo non-bloccante
 */
void wifi_time_init();

/**
 * Aggiorna lo stato WiFi (chiamare nel loop)
 * Gestisce riconnessione automatica
 */
void wifi_time_update();

/**
 * Forza risincronizzazione NTP
 */
void wifi_time_sync_now();

/**
 * Ottiene lo stato corrente del WiFi
 */
WiFiStatus wifi_time_get_status();

/**
 * Controlla se l'ora e' stata sincronizzata
 */
bool wifi_time_is_synced();

/**
 * Aggiorna le variabili di stato del forno con l'ora corrente
 * Chiamare dopo oven_state_init()
 */
void wifi_time_update_state();

/**
 * Ottiene l'indirizzo IP (stringa statica)
 */
const char* wifi_time_get_ip();

/**
 * Ottiene la potenza del segnale WiFi in dBm
 */
int wifi_time_get_rssi();

/**
 * Riconnetti WiFi manualmente (per OTA o test)
 * Usa credenziali da wifi_storage
 */
void wifi_time_reconnect();

/**
 * Disconnetti WiFi
 */
void wifi_time_disconnect();

/**
 * Mantieni WiFi connesso (non disconnettere dopo NTP sync)
 * Usato per OTA mode
 */
void wifi_time_keep_connected(bool keep);

/**
 * Controlla se WiFi e' attualmente connesso
 */
bool wifi_time_is_connected();

#endif // WIFI_TIME_H
