/**
 * WiFi and NTP Time Implementation
 * Uses wifi_storage for credentials
 */

#include "wifi_time.h"
#include "wifi_config.h"
#include "wifi_storage.h"
#include "settings_storage.h"
#include "oven_state.h"

#include <WiFi.h>
#include <ESPmDNS.h>
#include <time.h>

// ============================================
// State
// ============================================
static WiFiStatus s_status = WiFiStatus::DISCONNECTED;
static bool s_time_synced = false;          // Ora sincronizzata (almeno una volta)
static bool s_wifi_done = false;            // WiFi disconnesso dopo sync (non ritentare)
static bool s_ntp_waiting = false;          // In attesa di risposta NTP
static bool s_keep_connected = false;       // Mantieni connesso (per OTA)
static unsigned long s_connect_start = 0;
static unsigned long s_ntp_start = 0;       // Quando abbiamo iniziato a aspettare NTP
static unsigned long s_last_sync = 0;
static char s_ip_str[16] = "0.0.0.0";
static char s_current_ssid[WIFI_SSID_MAX_LEN + 1] = {0};
static bool s_mdns_started = false;
static unsigned long s_last_reconnect_attempt = 0;

#define NTP_TIMEOUT_MS 30000  // Max attesa NTP (non-bloccante, può aspettare)
#define RECONNECT_INTERVAL_MS 30000  // Auto-reconnect ogni 30s

// ============================================
// Internal: get credentials
// ============================================
static void get_credentials(char* ssid, char* password) {
    if (wifi_storage_has_credentials()) {
        wifi_storage_get_ssid(ssid, WIFI_SSID_MAX_LEN + 1);
        wifi_storage_get_password(password, WIFI_PASSWORD_MAX_LEN + 1);
    } else {
        // Fallback to hardcoded defaults
        strncpy(ssid, WIFI_SSID_DEFAULT, WIFI_SSID_MAX_LEN);
        strncpy(password, WIFI_PASSWORD_DEFAULT, WIFI_PASSWORD_MAX_LEN);
    }
}

// ============================================
// Initialize WiFi and NTP
// ============================================
void wifi_time_init() {
    Serial.println("WiFi: Inizializzazione...");

    // Get credentials
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASSWORD_MAX_LEN + 1];
    get_credentials(ssid, password);

    // Nessuna rete configurata: niente tentativi a vuoto, si imposta dal touch
    if (ssid[0] == '\0') {
        Serial.println("WiFi: nessuna rete configurata (Impostazioni > WiFi)");
        s_status = WiFiStatus::FAILED;
        return;
    }

    // Save current SSID for display
    strncpy(s_current_ssid, ssid, WIFI_SSID_MAX_LEN);

    // Configura NTP (prima della connessione)
    configTzTime(TZ_INFO, NTP_SERVER);

    // Avvia connessione WiFi (non-bloccante)
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    s_status = WiFiStatus::CONNECTING;
    s_connect_start = millis();
    s_wifi_done = false;

    Serial.printf("WiFi: Connessione a '%s'...\n", ssid);
}

// ============================================
// Update WiFi state (call in loop) - NON-BLOCCANTE
// ============================================
void wifi_time_update() {
    bool stay_connected = s_keep_connected || settings_get_wifi_always_on();

    // Se abbiamo gia' sincronizzato e disconnesso, e non dobbiamo restare connessi
    if (s_wifi_done && !stay_connected) return;

    unsigned long now = millis();

    switch (s_status) {
        case WiFiStatus::CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                // Connesso!
                s_status = WiFiStatus::CONNECTED;
                IPAddress ip = WiFi.localIP();
                snprintf(s_ip_str, sizeof(s_ip_str), "%d.%d.%d.%d",
                         ip[0], ip[1], ip[2], ip[3]);
                Serial.printf("WiFi: Connesso! IP: %s\n", s_ip_str);

                // Avvia mDNS (pyro.local)
                if (!s_mdns_started) {
                    if (MDNS.begin("pyro")) {
                        MDNS.addService("http", "tcp", 80);
                        s_mdns_started = true;
                        Serial.println("mDNS: pyro.local attivo");
                    }
                }

                // Se in keep_connected o always-on, non fare NTP solo se gia' sincronizzato
                if (stay_connected && s_time_synced) {
                    s_wifi_done = false;
                    return;
                }

                // Inizia attesa NTP (non-bloccante)
                s_ntp_waiting = true;
                s_ntp_start = now;
                Serial.println("NTP: Attesa sincronizzazione (non-bloccante)...");
            }
            else if (now - s_connect_start >= WIFI_TIMEOUT_MS) {
                // Timeout WiFi
                if (stay_connected) {
                    // Always-on: riprova dopo intervallo
                    s_status = WiFiStatus::DISCONNECTED;
                    s_wifi_done = false;
                    s_last_reconnect_attempt = now;
                    Serial.println("WiFi: Timeout. Riprovero' tra 30s...");
                } else {
                    s_status = WiFiStatus::FAILED;
                    s_wifi_done = true;
                    Serial.println("WiFi: Timeout. Continuo senza sincronizzazione ora.");
                }
            }
            break;

        case WiFiStatus::CONNECTED:
            // Check if still connected
            if (WiFi.status() != WL_CONNECTED) {
                s_status = WiFiStatus::DISCONNECTED;
                s_ip_str[0] = '0'; s_ip_str[1] = '\0';
                s_mdns_started = false;
                s_wifi_done = false;
                s_last_reconnect_attempt = now;
                Serial.println("WiFi: Disconnesso inaspettatamente");
                return;
            }

            // In attesa di NTP (polling non-bloccante)
            if (s_ntp_waiting) {
                struct tm timeinfo;
                if (getLocalTime(&timeinfo, 0)) {  // 0 = no wait
                    // Ora sincronizzata!
                    s_time_synced = true;
                    s_ntp_waiting = false;
                    s_last_sync = now;
                    Serial.printf("NTP: Sincronizzato! %02d/%02d/%04d %02d:%02d:%02d\n",
                                 timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900,
                                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);

                    // Se stay_connected, non disconnettere
                    if (stay_connected) {
                        s_wifi_done = false;
                        return;
                    }

                    // Disconnetti WiFi
                    Serial.println("WiFi: Ora sincronizzata, disconnessione...");
                    WiFi.disconnect(true);
                    WiFi.mode(WIFI_OFF);
                    s_status = WiFiStatus::DISCONNECTED;
                    s_wifi_done = true;
                    Serial.println("WiFi: Disconnesso. Timer interno attivo.");
                }
                else if (now - s_ntp_start >= NTP_TIMEOUT_MS) {
                    // Timeout NTP
                    s_ntp_waiting = false;

                    if (stay_connected) {
                        // Resta connesso anche senza NTP
                        s_wifi_done = false;
                        Serial.println("NTP: Timeout. WiFi resta connesso.");
                    } else {
                        s_wifi_done = true;
                        WiFi.disconnect(true);
                        WiFi.mode(WIFI_OFF);
                        s_status = WiFiStatus::FAILED;
                        Serial.println("NTP: Timeout. Continuo senza sincronizzazione ora.");
                    }
                }
            }
            break;

        case WiFiStatus::DISCONNECTED:
        case WiFiStatus::FAILED:
            // Auto-reconnect se always-on o keep_connected
            if (stay_connected && (now - s_last_reconnect_attempt >= RECONNECT_INTERVAL_MS)) {
                s_last_reconnect_attempt = now;
                Serial.println("WiFi: Auto-reconnect...");
                wifi_time_reconnect();
            }
            break;
    }
}

// ============================================
// Force NTP sync (solo per uso manuale, non usata in automatico)
// ============================================
void wifi_time_sync_now() {
    // La sincronizzazione automatica avviene in wifi_time_update()
    // Questa funzione è mantenuta per compatibilità
    Serial.println("NTP: Usa wifi_time_update() per sync non-bloccante");
}

// ============================================
// Get WiFi status
// ============================================
WiFiStatus wifi_time_get_status() {
    return s_status;
}

// ============================================
// Check if time is synced
// ============================================
bool wifi_time_is_synced() {
    return s_time_synced;
}

// ============================================
// Update oven state with current time
// ============================================
void wifi_time_update_state() {
    if (!s_time_synced) return;

    // Throttle to 1x/sec (getLocalTime has overhead even with timeout=0)
    static unsigned long s_last_time_read = 0;
    unsigned long now = millis();
    if (now - s_last_time_read < 1000) return;
    s_last_time_read = now;

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 0)) return;  // 0 = non-blocking

    // Aggiorna lo stato del forno
    g_state.hour = timeinfo.tm_hour;
    g_state.minute = timeinfo.tm_min;
    g_state.second = timeinfo.tm_sec;

    // tm_wday: 0 = Domenica, 1 = Lunedi, ... (come il nostro formato)
    g_state.day_of_week = timeinfo.tm_wday;

    g_state.day = timeinfo.tm_mday;
    g_state.month = timeinfo.tm_mon + 1;  // tm_mon e' 0-based
    g_state.year = timeinfo.tm_year + 1900;
}

// ============================================
// Get IP address string
// ============================================
const char* wifi_time_get_ip() {
    return s_ip_str;
}

// ============================================
// Get WiFi signal strength
// ============================================
int wifi_time_get_rssi() {
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

// ============================================
// Reconnect WiFi manually
// ============================================
void wifi_time_reconnect() {
    Serial.println("WiFi: Riconnessione manuale...");

    // Disconnect first if connected
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.disconnect(true);
    }

    // Get credentials
    char ssid[WIFI_SSID_MAX_LEN + 1];
    char password[WIFI_PASSWORD_MAX_LEN + 1];
    get_credentials(ssid, password);
    if (ssid[0] == '\0') {
        s_status = WiFiStatus::FAILED;
        return;
    }

    // Save current SSID for display
    strncpy(s_current_ssid, ssid, WIFI_SSID_MAX_LEN);

    // Configura NTP
    configTzTime(TZ_INFO, NTP_SERVER);

    // Avvia connessione WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    s_status = WiFiStatus::CONNECTING;
    s_connect_start = millis();
    s_wifi_done = false;
    s_ntp_waiting = false;

    Serial.printf("WiFi: Connessione a '%s'...\n", ssid);
}

// ============================================
// Disconnect WiFi
// ============================================
void wifi_time_disconnect() {
    Serial.println("WiFi: Disconnessione manuale...");

    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);

    s_status = WiFiStatus::DISCONNECTED;
    s_wifi_done = true;
    s_keep_connected = false;
    s_ip_str[0] = '0';
    s_ip_str[1] = '.';
    s_ip_str[2] = '0';
    s_ip_str[3] = '.';
    s_ip_str[4] = '0';
    s_ip_str[5] = '.';
    s_ip_str[6] = '0';
    s_ip_str[7] = '\0';

    Serial.println("WiFi: Disconnesso.");
}

// ============================================
// Keep WiFi connected (for OTA mode)
// ============================================
void wifi_time_keep_connected(bool keep) {
    s_keep_connected = keep;
    if (keep) {
        s_wifi_done = false;
        Serial.println("WiFi: Keep-connected mode ATTIVO");
    } else {
        Serial.println("WiFi: Keep-connected mode DISATTIVO");
    }
}

// ============================================
// Check if WiFi is currently connected
// ============================================
bool wifi_time_is_connected() {
    return (WiFi.status() == WL_CONNECTED);
}
