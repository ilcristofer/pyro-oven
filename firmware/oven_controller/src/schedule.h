/**
 * Avvio programmato
 * Accende il forno a un orario con temperatura e timer propri. Il timer parte
 * appena le sonde raggiungono la temperatura (senza aspettare la pietra) e a
 * timer scaduto il forno si spegne. Vale una volta sola: all'avvio la
 * programmazione si disattiva. Nessuna chiamata LVGL: sicuro anche dal web.
 */

#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <Arduino.h>

enum class ScheduleState : uint8_t {
    OFF,      // nessuna programmazione
    ARMED,    // in attesa dell'orario
    HEATING,  // forno acceso, timer fermo finche' non arriva in temperatura
    TIMING,   // timer in corso: a zero il forno si spegne
};

#define SCHEDULE_DEFAULT_HOUR   19
#define SCHEDULE_DEFAULT_MINUTE 0
#define SCHEDULE_DEFAULT_DUR_S  (30 * 60)
#define SCHEDULE_MAX_DUR_S      (99 * 60 + 59)

// Avvio scoperto in ritardo (forno senza corrente o ancora senza ora NTP):
// parte solo entro questa finestra, poi si considera saltato
#define SCHEDULE_LATE_MAX_S     (15 * 60)

// Nessuno davanti al forno: se la temperatura non arriva entro questo tempo
// si spegne invece di restare acceso all'infinito
#define SCHEDULE_HEAT_MAX_MIN   180

void schedule_init();

// Ogni giro del loop, subito dopo oven_state_update_timer(): la scadenza del
// timer (timer_expired) va vista prima che un tocco la cancelli
void schedule_update();

// Arma per il prossimo orario (oggi o domani). False se l'ora non e'
// sincronizzata, se la durata e' zero o se un avvio e' gia' in corso
bool schedule_arm();

// Disattiva. Un avvio in corso lascia il forno acceso: si ferma solo
// l'automatismo (torna lo spegnimento per inattivita')
void schedule_cancel();

// Con la programmazione armata ricalcolano l'orario di avvio
void schedule_set_time(int hour, int minute);
void schedule_set_temp(int temp);
void schedule_set_duration_s(int seconds);

ScheduleState schedule_get_state();
bool schedule_is_running();          // HEATING o TIMING
int  schedule_get_hour();
int  schedule_get_minute();
int  schedule_get_temp();
int  schedule_get_duration_s();
int  schedule_minutes_to_start();    // -1 se non armata
const char* schedule_get_note();     // esito dell'ultimo avvio ("" = nessuno)

#endif // SCHEDULE_H
