/**
 * Hardware Pin Definitions for WT32-SC01
 * Forno Pizza Project
 */

#ifndef HARDWARE_PINS_H
#define HARDWARE_PINS_H

// ============================================
// Display SPI (ST7796)
// ============================================
#define TFT_SCLK    14
#define TFT_MOSI    13
#define TFT_DC      21
#define TFT_CS      15
#define TFT_RST     22
#define TFT_BL      23  // Backlight PWM

// ============================================
// Touch I2C (FT5x06)
// ============================================
#define TOUCH_SDA   18
#define TOUCH_SCL   19
#define TOUCH_INT   39
#define TOUCH_ADDR  0x38

// ============================================
// MAX31855 Thermocouples (SPI)
// ============================================
#define MAX_CLK         33
#define MAX_DO          32
#define MAX_CS_CIELO    26  // Top heating element  - filo BIANCO
#define MAX_CS_PLATEA   27  // Bottom heating element - filo MARRONE

// ============================================
// SSR Control (posizioni dal BASSO del connettore)
// ============================================
#define SSR_CIELO   12  // Top SSR    - filo VERDE (colonna DX, 12° dal basso)
#define SSR_PLATEA  2   // Bottom SSR - filo VIOLA (colonna SX, 6° dal basso)
// GND comune SSR - filo BLU (colonna SX, 4° dal basso)

// ============================================
// Buzzer (passive, PWM driven)
// ============================================
#define BUZZER_PIN  4

// ============================================
// Ventola raffreddamento 4-pin PWM (box cooling)
// ============================================
#define FAN_PIN     0       // GPIO0 - filo BLU ventola PWM (IO0/I2S_MCLK, boot pin - ok dopo boot)
#define FAN_PWM_FREQ     25000  // 25kHz - frequenza standard ventole PC
#define FAN_PWM_CHANNEL  2      // LEDC channel (0,1 usati da buzzer/backlight)
#define FAN_PWM_RES      8      // 8-bit resolution (0-255)

// Fan TACH input (segnale tachimetro, 2 impulsi/giro)
#define FAN_TACH_PIN            5       // GPIO5 - filo VERDE ventola (pull-up interno)
#define FAN_TACH_INTERVAL       2000    // Intervallo lettura RPM (ms) - uguale a SENSOR_READ_INTERVAL
#define FAN_RPM_MIN_THRESHOLD   200     // RPM minimo sotto il quale la ventola è considerata ferma
#define FAN_PWM_MIN_FOR_CHECK   30      // PWM minimo per attivare il monitoraggio (~12%)
#define FAN_FAILURE_COUNT       5       // Letture consecutive sotto soglia prima di errore
#define FAN_STARTUP_GRACE_MS    4000    // Tempo di grazia dopo cambio velocità (spinup)
#define FAN_PULSES_PER_REV      2       // Impulsi per giro (standard ventole PC)

// ============================================
// Preheat Power (cielo arriva prima, rallentiamo)
// ============================================
#define PREHEAT_POWER_CIELO   90    // 90% cielo (arriva prima, rallentiamo)
#define PREHEAT_POWER_PLATEA  100   // 100% platea

// ============================================
// Recovery Boost (porta aperta / calo termico)
// ============================================
#define RECOVERY_THRESHOLD    5     // °C sotto target per attivare recovery 100%

// ============================================
// Infornate in sequenza (modalita' servizio)
// ============================================
// La sonda CIELO sta in camera: all'apertura della porta e all'ingresso della
// pizza reagisce in secondi, molto prima della sonda sotto la pietra. E' quindi
// lei a far scattare il recupero.
// Soglia tarata su 6 infornate reali a 380C (sessione 2026-09-16):
//   calo del cielo su 20s a forno chiuso: max 2 C
//   calo del cielo su 20s con la pizza dentro: 5-13 C
// La finestra di 20s separa meglio di quella da 10s (dove il calo con pizza
// arrivava solo a 7 C) e il calo risulta graduale, non brusco.
#define INFORNATA_DROP_C      6     // °C persi in ~20s sul cielo = infornata
#define RECOVERY_MAX_SECONDS  300   // tetto di durata del recupero a piena potenza
#define SERVICE_AUTO_OFF_MIN  90    // il servizio si disattiva da solo dopo N min

// Scostamento dal target oltre il quale il forno smette di dirsi "pronto" e
// mostra quanto manca: a 380C recupera 3.7 C/min, quindi la differenza conta
#define TEMP_DEFICIT_WARN     10    // °C sotto il target

// ============================================
// Limiti di sicurezza
// ============================================
// Tetto assoluto: sopra questa temperatura nessuna resistenza resta accesa,
// boost compreso. I target arrivano a 500, il cielo supera il target di pochi
// gradi: 530 lo si tocca solo per un guasto (sonda fuori posto, SSR, bug)
// Versione mostrata all'avvio e nella schermata di aggiornamento
#define FW_VERSION            "3.29"

#define TEMP_ABS_MAX          530   // °C
// Il boost scalda oltre il termostato, ma non senza limite: in SEMPLICE non ha
// countdown e ce se ne puo' dimenticare acceso
#define BOOST_MAX_OVER        30    // °C oltre il target a cui il boost si ferma
// Scheda elettronica troppo calda (giunto freddo dei MAX31855)
#define BOX_TEMP_MAX          70    // °C

// A forno spento sopra questa temperatura lo schermo avvisa "FORNO CALDO"
#define HOT_WARN_TEMP         60    // °C

// ============================================
// Display Constants
// ============================================
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  480

// ============================================
// Timing Constants
// ============================================
#define SENSOR_READ_INTERVAL    2000  // ms (2 sec - sufficiente per inerzia termica forno)
#define ANIMATION_INTERVAL      400   // ms for resistance blink
#define CLOCK_TICK_INTERVAL     1000  // ms

#endif // HARDWARE_PINS_H
