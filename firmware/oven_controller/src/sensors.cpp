/**
 * Temperature Sensors Implementation
 */

#include "sensors.h"
#include "hardware_pins.h"
#include "oven_state.h"
#include <SPI.h>
#include <Adafruit_MAX31855.h>

// ============================================
// Sensor instances
// ============================================
static Adafruit_MAX31855 thermo_cielo(MAX_CLK, MAX_CS_CIELO, MAX_DO);
static Adafruit_MAX31855 thermo_platea(MAX_CLK, MAX_CS_PLATEA, MAX_DO);

// Error threshold (consecutive errors before reporting)
#define ERROR_THRESHOLD 5
// Letture buone consecutive per uscire dall'errore: con un contatto
// intermittente una sola lettura buona ogni tanto azzerava tutto e l'errore
// non veniva mai dichiarato
#define RECOVERY_THRESHOLD_READS 3
// Intervallo plausibile per una termocoppia K in questo forno
#define TEMP_PLAUSIBLE_MIN  -20.0
#define TEMP_PLAUSIBLE_MAX  700.0
// Salto massimo credibile tra due letture (2s): il forno non cambia di 50C in
// 2s, un disturbo sulla SPI software si'. Un salto isolato viene scartato
#define TEMP_MAX_JUMP       50.0

// Stato del filtro per ciascuna sonda
struct SensorFilter {
    bool has_last;       // c'e' una lettura valida di riferimento
    double last;         // ultima lettura accettata
    int good_count;      // letture buone consecutive (per uscire dall'errore)
};
static SensorFilter s_filter_cielo = {false, 0, 0};
static SensorFilter s_filter_platea = {false, 0, 0};

// ============================================
// Initialize sensors
// ============================================
void sensors_init() {
    Serial.print("Initializing CIELO sensor... ");
    if (!thermo_cielo.begin()) {
        Serial.println("FAILED!");
    } else {
        Serial.println("OK");
    }

    Serial.print("Initializing PLATEA sensor... ");
    if (!thermo_platea.begin()) {
        Serial.println("FAILED!");
    } else {
        Serial.println("OK");
    }
}

// ============================================
// Process a single sensor reading
// ============================================
static void process_sensor_reading(Adafruit_MAX31855 &sensor, SensorFilter &f,
                                   int &error_count, bool &error_flag,
                                   char* error_msg, size_t msg_size, int &temp_out) {
    double t = sensor.readCelsius();
    const char* fault = nullptr;

    if (isnan(t)) {
        uint8_t e = sensor.readError();
        if (e & MAX31855_FAULT_OPEN) {
            fault = "Disconnected";
        } else if (e & MAX31855_FAULT_SHORT_GND) {
            fault = "Short GND";
        } else if (e & MAX31855_FAULT_SHORT_VCC) {
            fault = "Short VCC";
        } else {
            fault = "Unknown";
        }
    } else if (t == 0.0 && sensor.readInternal() == 0.0) {
        // Parola SPI tutta a zero: la libreria la legge come 0C valido, ma e'
        // la linea DO bloccata bassa o il chip senza alimentazione. Presa per
        // buona mandava le resistenze al 100% a forno caldo
        fault = "Bus SPI";
    } else if (t < TEMP_PLAUSIBLE_MIN || t > TEMP_PLAUSIBLE_MAX) {
        fault = "Fuori scala";
    } else if (!error_flag && f.has_last && fabs(t - f.last) > TEMP_MAX_JUMP) {
        // Salto non credibile: se si ripete per ERROR_THRESHOLD letture
        // diventa errore, altrimenti la lettura dopo lo smentisce
        fault = "Instabile";
    }

    if (fault) {
        f.good_count = 0;
        error_count++;
        if (error_count >= ERROR_THRESHOLD) {
            error_flag = true;
            strncpy(error_msg, fault, msg_size - 1);
            error_msg[msg_size - 1] = '\0';
        }
        return;
    }

    f.has_last = true;
    f.last = t;
    temp_out = (int)round(t);
    error_count = 0;

    if (error_flag) {
        f.good_count++;
        if (f.good_count >= RECOVERY_THRESHOLD_READS) {
            error_flag = false;
            error_msg[0] = '\0';
            f.good_count = 0;
        }
    }
}

// ============================================
// Read temperatures
// ============================================

void sensors_read() {
    process_sensor_reading(thermo_cielo, s_filter_cielo,
                           g_state.error_count_cielo, g_state.error_cielo,
                           g_state.error_msg_cielo, sizeof(g_state.error_msg_cielo),
                           g_state.temp_cielo);

    process_sensor_reading(thermo_platea, s_filter_platea,
                           g_state.error_count_platea, g_state.error_platea,
                           g_state.error_msg_platea, sizeof(g_state.error_msg_platea),
                           g_state.temp_platea);

    // Read cold junction temperatures (ambient/box temperature)
    double internal_cielo = thermo_cielo.readInternal();
    double internal_platea = thermo_platea.readInternal();

    // Average of both sensors for better accuracy
    if (!isnan(internal_cielo) && !isnan(internal_platea)) {
        g_state.temp_ambient = (int)round((internal_cielo + internal_platea) / 2.0);
    } else if (!isnan(internal_cielo)) {
        g_state.temp_ambient = (int)round(internal_cielo);
    } else if (!isnan(internal_platea)) {
        g_state.temp_ambient = (int)round(internal_platea);
    }

    Serial.printf("CIELO: %d C  PLATEA: %d C  BOX: %d C\n",
                  g_state.temp_cielo, g_state.temp_platea, g_state.temp_ambient);
}
