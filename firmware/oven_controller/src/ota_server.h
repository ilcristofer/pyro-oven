/**
 * OTA Update Server
 * HTTP server for firmware uploads
 */

#ifndef OTA_SERVER_H
#define OTA_SERVER_H

#include <Arduino.h>

// ============================================
// OTA Status
// ============================================
enum class OtaStatus {
    IDLE,           // Server non attivo
    WAITING,        // Server attivo, in attesa di upload
    UPLOADING,      // Upload in corso
    SUCCESS,        // Upload completato con successo
    ERROR           // Errore durante upload
};

// ============================================
// OTA Server Functions
// ============================================

/**
 * Start OTA web server on port 80
 * Requires WiFi to be connected
 */
void ota_server_start();

/**
 * Stop OTA web server
 */
void ota_server_stop();

/**
 * Handle server requests (call in loop)
 */
void ota_server_update();

/**
 * Check if server is running
 */
bool ota_server_is_running();

/**
 * Get current OTA status
 */
OtaStatus ota_server_get_status();

/**
 * Get upload progress (0-100)
 */
int ota_server_get_progress();

/**
 * Get error message (if status == ERROR)
 */
const char* ota_server_get_error();

/**
 * Reset status to IDLE (after showing success/error)
 */
void ota_server_reset_status();

/**
 * Perform reboot (call after SUCCESS)
 */
void ota_server_reboot();

#endif // OTA_SERVER_H
