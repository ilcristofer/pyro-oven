/**
 * WiFi Credentials Storage
 * Stores SSID and password in NVS for persistence across reboots
 */

#ifndef WIFI_STORAGE_H
#define WIFI_STORAGE_H

#include <Arduino.h>

#define WIFI_SSID_MAX_LEN     32
#define WIFI_PASSWORD_MAX_LEN 64

/**
 * Initialize WiFi storage
 * Loads credentials from NVS if available
 */
void wifi_storage_init();

/**
 * Check if valid credentials are stored
 */
bool wifi_storage_has_credentials();

/**
 * Get stored SSID
 * @param buf Buffer to store SSID (must be at least WIFI_SSID_MAX_LEN+1)
 * @param len Buffer length
 */
void wifi_storage_get_ssid(char* buf, size_t len);

/**
 * Get stored password
 * @param buf Buffer to store password (must be at least WIFI_PASSWORD_MAX_LEN+1)
 * @param len Buffer length
 */
void wifi_storage_get_password(char* buf, size_t len);

/**
 * Save new credentials to NVS
 */
void wifi_storage_save(const char* ssid, const char* password);

/**
 * Clear stored credentials
 */
void wifi_storage_clear();

#endif // WIFI_STORAGE_H
