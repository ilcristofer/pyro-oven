/**
 * Buzzer Driver for Passive Buzzer (PWM)
 * Forno Pizza Project
 */

#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

// Frequenze dei toni (Hz)
#define BUZZER_FREQ_LOW     800
#define BUZZER_FREQ_MID     1200
#define BUZZER_FREQ_HIGH    1800
#define BUZZER_FREQ_ALERT   1600

// Note musicali per melodie
#define NOTE_C4  262
#define NOTE_D4  294
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_G4  392
#define NOTE_A4  440
#define NOTE_B4  494
#define NOTE_C5  523
#define NOTE_D5  587
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_G5  784
#define NOTE_A5  880
#define NOTE_B5  988
#define NOTE_Fs4 370
#define NOTE_Cs5 554
#define NOTE_Fs5 740

// Durate (ms)
#define BUZZER_DURATION_SHORT   50
#define BUZZER_DURATION_MEDIUM  150
#define BUZZER_DURATION_LONG    500

// Volume range (0-100)
#define BUZZER_VOLUME_MIN   10
#define BUZZER_VOLUME_MAX   100
#define BUZZER_VOLUME_DEFAULT 70

/**
 * Inizializza il buzzer (configura LEDC PWM)
 */
void buzzer_init();

/**
 * Aggiorna lo stato del buzzer (chiamare nel loop principale)
 * Gestisce la temporizzazione non-bloccante dei suoni
 */
void buzzer_update();

/**
 * Melodia di avvio - suona all'accensione
 */
void buzzer_startup();

/**
 * Beep singolo breve - feedback pressione tasto
 */
void buzzer_beep();

/**
 * Doppio beep - conferma azione (es. setpoint impostato)
 */
void buzzer_confirm();

/**
 * Tono lungo - timer scaduto o evento importante
 */
void buzzer_alert();

/**
 * Beep ripetuto urgente - allarme (es. sovratemperatura)
 */
void buzzer_error();

/**
 * Suona un tono personalizzato
 * @param freq Frequenza in Hz
 * @param duration Durata in ms
 */
void buzzer_tone(uint16_t freq, uint16_t duration);

/**
 * Suona il ritornello di "L'amour Toujours" (Gigi D'Agostino)
 * Non-bloccante - utilizza la state machine del buzzer
 */
void buzzer_lamour_toujours();

/**
 * Ferma immediatamente il buzzer
 */
void buzzer_stop();

/**
 * Abilita/disabilita il buzzer globalmente
 */
void buzzer_set_enabled(bool enabled);

/**
 * Verifica se il buzzer è abilitato
 */
bool buzzer_is_enabled();

/**
 * Imposta il volume (0-100)
 */
void buzzer_set_volume(uint8_t volume);

/**
 * Ottiene il volume corrente (0-100)
 */
uint8_t buzzer_get_volume();

#endif // BUZZER_H
