/**
 * Preavviso di spegnimento automatico per inattivita'
 * Un riquadro sopra qualsiasi schermata (layer top) negli ultimi
 * INACTIVITY_WARN_S secondi, con un allarme all'inizio. Qualsiasi tocco lo
 * chiude e fa ripartire il conteggio.
 */

#ifndef UI_INACTIVITY_H
#define UI_INACTIVITY_H

// Chiamare a 1Hz dal main loop
void ui_inactivity_update();

#endif // UI_INACTIVITY_H
