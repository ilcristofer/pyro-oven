/**
 * WiFi Credentials Storage Implementation
 * Uses NVS (Preferences) for persistence
 */

#include "wifi_storage.h"
#include <Preferences.h>

#define PREFS_WIFI_NS "wifi"
#define KEY_SSID      "ssid"
#define KEY_PASSWORD  "pass"
#define KEY_CONFIGURED "cfg"

static Preferences s_prefs;
static char s_ssid[WIFI_SSID_MAX_LEN + 1] = {0};
static char s_password[WIFI_PASSWORD_MAX_LEN + 1] = {0};
static bool s_has_credentials = false;

void wifi_storage_init() {
    s_prefs.begin(PREFS_WIFI_NS, true);  // Read-only

    s_has_credentials = s_prefs.getBool(KEY_CONFIGURED, false);

    if (s_has_credentials) {
        s_prefs.getString(KEY_SSID, s_ssid, sizeof(s_ssid));
        s_prefs.getString(KEY_PASSWORD, s_password, sizeof(s_password));

        // Validate - SSID must not be empty
        if (strlen(s_ssid) == 0) {
            s_has_credentials = false;
        }
    }

    s_prefs.end();

    Serial.printf("WiFi Storage: %s\n",
        s_has_credentials ? "Credentials loaded" : "No credentials stored");
    if (s_has_credentials) {
        Serial.printf("  SSID: %s\n", s_ssid);
    }
}

bool wifi_storage_has_credentials() {
    return s_has_credentials;
}

void wifi_storage_get_ssid(char* buf, size_t len) {
    strncpy(buf, s_ssid, len - 1);
    buf[len - 1] = '\0';
}

void wifi_storage_get_password(char* buf, size_t len) {
    strncpy(buf, s_password, len - 1);
    buf[len - 1] = '\0';
}

void wifi_storage_save(const char* ssid, const char* password) {
    // Update local cache
    strncpy(s_ssid, ssid, WIFI_SSID_MAX_LEN);
    s_ssid[WIFI_SSID_MAX_LEN] = '\0';

    strncpy(s_password, password, WIFI_PASSWORD_MAX_LEN);
    s_password[WIFI_PASSWORD_MAX_LEN] = '\0';

    s_has_credentials = (strlen(s_ssid) > 0);

    // Save to NVS
    s_prefs.begin(PREFS_WIFI_NS, false);  // Read-write
    s_prefs.putString(KEY_SSID, s_ssid);
    s_prefs.putString(KEY_PASSWORD, s_password);
    s_prefs.putBool(KEY_CONFIGURED, s_has_credentials);
    s_prefs.end();

    Serial.printf("WiFi Storage: Saved SSID '%s'\n", s_ssid);
}

void wifi_storage_clear() {
    s_ssid[0] = '\0';
    s_password[0] = '\0';
    s_has_credentials = false;

    s_prefs.begin(PREFS_WIFI_NS, false);
    s_prefs.clear();
    s_prefs.end();

    Serial.println("WiFi Storage: Credentials cleared");
}
