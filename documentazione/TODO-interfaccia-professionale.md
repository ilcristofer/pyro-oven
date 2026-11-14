# TODO: Interfaccia Professionale Forno Pizza

## Stato Progetto

### Completato

#### Hardware & Base
- [x] Test lettura 1 sonda MAX31855
- [x] Display funzionante con LovyanGFX
- [x] Interfaccia base con temperatura, target, tastierino
- [x] Gestione touch
- [x] Filtro errori sonda (5 letture consecutive)
- [x] Documentazione collegamenti hardware
- [x] Collegare seconda sonda MAX31855
- [x] Firmware supporto 2 sonde (CIELO + PLATEA)
- [x] Testare lettura seconda sonda

#### Interfaccia v2.0 (LovyanGFX)
- [x] Implementare interfaccia stile Effeuno v2.0
- [x] Indicatori fasi circolari (tachimetro)
- [x] Temperatura gigante centrale con barra grafica
- [x] SET/PWR in linea per cielo e platea
- [x] 8 pulsanti funzione su 2 righe
- [x] Funzione Boost (toggle cielo/platea)
- [x] Timer countdown con lampeggio
- [x] Animazione resistenze durante riscaldamento
- [x] Zone touch conformi (minimo 44px)

#### Migrazione LVGL v3.0
- [x] **MIGRAZIONE A LVGL 8.4.0**
- [x] Architettura modulare (stato separato da UI)
- [x] Widget LVGL nativi (lv_meter, lv_bar, lv_btnmatrix)
- [x] Tema scuro personalizzato
- [x] Tastierino numerico LVGL

#### Interfaccia Effeuno v3.1 (NUOVA)
- [x] **Layout esatto come manuale Effeuno Evolution Multi**
- [x] 3 indicatori fasi con arc 270° (temp superiore, tempo, temp inferiore)
- [x] Temperatura grande centrale (font 72pt)
- [x] Barra temperatura segmentata verticale (36 segmenti: rosso→giallo→verde)
- [x] Icone SET cielo/platea (rettangolo bianco con linea nera alta/bassa)
- [x] Icone resistenze con lampeggio rosso
- [x] Status bar con PROG/NOME/STATO
- [x] 8 pulsanti funzione (2x4 grid)
- [x] Pulsanti controllo: Restart, Play/Pause, Timer, Save
- [x] **Layout 4 colonne**: SET | Temp+Power | °C+Platea+Resistenze | Barra

#### WiFi e NTP v3.2 (NUOVA)
- [x] **Sincronizzazione ora via NTP**
- [x] Connessione WiFi automatica all'avvio
- [x] Disconnessione WiFi dopo sync (risparmio energia)
- [x] Icona WiFi nell'header (lampeggia durante connessione)
- [x] Data/ora visibile solo dopo sincronizzazione

#### Modalità Operative v3.1 (NUOVA)
- [x] **PREHEAT** - Preriscaldamento con potenze dedicate
- [x] **MAINTAIN** - Mantenimento con potenze ridotte
- [x] **BOOST** - Cielo e/o platea con countdown (max 180s)
- [x] **Luce camera** - Toggle ON/OFF
- [x] Transizione automatica PREHEAT → MAINTAIN (5°C dal target)
- [x] Auto-standby dopo 1 ora di inattività
- [x] Potenze separate: normale, preheat, maintain, boost
- [x] Timer cottura con pausa

#### Interfaccia v3.6 (NUOVA)
- [x] **Icone resistenze con caps orizzontali** (zigzag 9 punti)
- [x] Icone PREHEAT/MAINTAIN/BOOST stesso stile resistenze
- [x] Icone resistenze sotto platea con caps 5px
- [x] MENU: LV_SYMBOL_LIST (3 linee orizzontali)
- [x] **Stati pulsanti attivi con sfondo colorato e icone nere**
- [x] PREHEAT attivo: sfondo arancione
- [x] MAINTAIN attivo: sfondo giallo
- [x] BOOST attivo: sfondo rosso
- [x] POWER attivo: sfondo verde
- [x] LIGHT attivo: bordo e icona gialla
- [x] **Status bar colori dinamici per stato**
- [x] Countdown temperatura al posto del nome programma
- [x] Boost mostra 100% nella visualizzazione potenza
- [x] Bordo bianco per tutti i pulsanti quando premuti

#### Audio e Impostazioni v3.7 (NUOVA)
- [x] **Buzzer passivo su GPIO4** (PWM driven)
- [x] Beep feedback su tutti i pulsanti UI
- [x] Beep su tasti keypad numerico
- [x] Doppio beep conferma su keypad OK
- [x] **Allarme sonoro quando timer scade** (3 toni)
- [x] Tono breve all'avvio
- [x] **Pagina impostazioni** raggiungibile da pulsante MENU
- [x] Toggle suono ON/OFF con switch
- [x] Slider volume (10-100%)
- [x] **Icona suono nell'header** (volume/mute, sempre bianca)
- [x] Abilitati widget LVGL: slider, switch

#### Persistenza e Programmi v3.8 (NUOVA)
- [x] **WiFi/NTP non bloccante** - interfaccia reattiva all'avvio (timeout 30s)
- [x] **Persistenza impostazioni audio** su NVS (settings_storage.cpp)
- [x] **Reset di fabbrica** in pagina impostazioni
- [x] **Sistema 20 programmi (P0-P19)** con persistenza NVS
- [x] **P0 (MANUALE)** - programma speciale:
  - Sempre caricato all'avvio
  - Auto-salva modifiche da main screen
  - Editabile anche da Gestione Programmi
- [x] **P1-P5 preimpostati**: Napoletana, Romana, Teglia, Americana, Focaccia
- [x] **Schermata selezione programmi** (pulsante P):
  - Lista paginata (10 items/pagina, 2 pagine)
  - Tap su programma = seleziona e torna
- [x] Struct `Program` in oven_state.h
- [x] programs_storage.cpp per persistenza NVS
- [x] **ui_program_edit.cpp** - schermata modifica programmi completa
- [x] **ui_keyboard.cpp** - tastiera alfanumerica QWERTY

### Da Fare - Priorita' Alta
- [x] ~~Controllo potenza resistenze (SSR)~~ - **Completato v3.12**
- [x] ~~OTA Firmware Update~~ - **Completato v3.15**
- [x] ~~Configurazione WiFi da UI~~ - **Completato v3.15**
- [x] ~~Calibrazione forno~~ - **Sostituita da auto-learning v3.22**
- [x] ~~Programmi cottura~~ - **Completato v3.21**

#### Interfaccia Web Programmi v3.25 (NUOVA)
- [x] **Redesign pagina /programs** (ota_server.cpp PROGRAMS_HTML)
- [x] Categorie colorate con bordo sinistro: ambra (P0), rosso (P1-P7 pizza), viola (P8-P18 cotture), grigio (P19)
- [x] Badge P-index colorato per categoria
- [x] Dot temperatura 8px colorato per range (blu <200, giallo 200-299, arancione 300-399, rosso 400+)
- [x] Tab filtro sticky TUTTI / PIZZA / COTTURE
- [x] Vista collassata arricchita: badge, dot, nome, info (temp/potenza/durata/fasi), chevron animato
- [x] Transizioni smooth CSS (max-height 0→700px) con rendering incrementale
- [x] Phase pills (3 bottoni toggle) al posto del select
- [x] Input numerici con min/max/step e inputmode="numeric" per tastiera mobile
- [x] Unita' di misura (°C, %, min, sec) accanto ai campi
- [x] Loading spinner all'avvio pagina
- [x] Save con stati (Salva → ... → Salvato! → Salva) e aggiornamento header locale
- [x] Toast notification fisso in basso (slide-up animato)
- [x] Reset two-step (Default → Conferma? con pulse 3s → esegui)
- [x] Validazione client-side (temperatura 0-500, potenza 0-100)
- [x] Navigazione: "← Menu principale", titolo PYRO arancione
- [x] Touch target minimi 48-52px, ottimizzato per mobile

### Da Fare - Priorita' Media
- [ ] Schermata Log Allarmi
- [ ] Import/Export programmi USB (PROGRAMS.CSV)

### Da Fare - Priorita' Bassa
- [ ] Icone grafiche custom (sostituire simboli LVGL)
- [ ] Migliorare gauge con tick marks grafici

---

## Hardware

### Scheda principale
- **WT32-SC01**: ESP32 con display touch 480x320 (verticale: 320x480)
- **Display**: ST7796, touch FT5x06
- **Libreria**: LovyanGFX + LVGL 8.4.0

### Sonde temperatura (2x MAX31855)

#### Collegamento con breadboard (segnali condivisi)

```
ESP32-SC01 (colonna 1 destra)     BREADBOARD          MAX31855
─────────────────────────────────────────────────────────────────
3V3 (2° pin dall'alto)     →     Riga 1, col.a   →   Vin #1 e #2
GND (3° pin dall'alto)     →     Riga 5, col.a   →   GND #1 e #2
GPIO33 (5° pin dall'alto)  →     Riga 10, col.a  →   CLK #1 e #2
GPIO32 (6° pin dall'alto)  →     Riga 15, col.a  →   DO #1 e #2
GPIO26 (col.2, 6° dall'alto) →   DIRETTO         →   CS #1 (CIELO)
GPIO27 (7° pin dall'alto)  →     DIRETTO         →   CS #2 (PLATEA)
```

#### Pin sonde
| Sonda | Posizione | GPIO CS | Colore filo | Funzione |
|-------|-----------|---------|-------------|----------|
| #1 CIELO | Camera cottura, sotto resistenza superiore | GPIO26 | **Bianco** | Temperatura ambiente camera |
| #2 PLATEA | Sotto pietra refrattaria | GPIO27 | **Marrone** | Temperatura base pizza |

### Controllo Resistenze (2x SSR)

#### Pin GPIO per SSR (SSR-40DA, 1000W per resistenza)
| SSR | Resistenza | GPIO | Colore filo | Posizione (dal basso) |
|-----|------------|------|-------------|----------------------|
| #1 | CIELO (sopra) | GPIO12 | **Verde** | Colonna DX, **12° dal basso** |
| #2 | PLATEA (sotto) | GPIO2 | **Viola** | Colonna SX, **6° dal basso** |
| GND | Massa comune | GND | **Blu** | Colonna SX, **4° dal basso** |

**Nota**: Burst firing con periodo 1s. Test con lampadine OK (2026-01-18).

### Buzzer

| Componente | GPIO | Funzione |
|------------|------|----------|
| Buzzer passivo | GPIO4 | Feedback acustico (PWM) |

---

## Interfaccia v3.1 - Layout Effeuno

### Ispirazione: Effeuno Evolution Multi
- Immagine: `documentazione/Effeuno-presenta-2.png`
- Manuale: `documentazione/effeuno_libretto_a4_evolution_multi_01_2024.pdf`

### Layout Verticale 320x480

```
┌────────────────────────────────────┐ Y=0
│ 📶 MARTEDI' 07.02.2023 - 12:52 [PWR]│ 35px - Header + WiFi icon
├────────────────────────────────────┤ Y=35
│    155°       156°       157°      │
│   ⌒2:00⌒    ⌒2:00⌒    ⌒2:00⌒    │ 105px - 3 Fasi circolari
│    155°       156°       157°      │
├────────────────────────────────────┤ Y=140
│ PROG. 5 │ EFFEUNO P5 │   STANDBY  │ 28px - Status bar
├────────────────────────────────────┤ Y=168
│ COL1    │  COL2      │ COL3 │COL4 │
│ SET     │            │  °C  │ ▌   │
│ [▀▀]    │    300     │ [▄▄] │ ▌   │ 152px - 4 colonne
│ 155°    │            │ 300° │ ▌   │ Temp+Power section
│ SET     │   75 %     │ ∿∿∿  │ ▌   │
│ [▄▄]    │ ────────   │ ∿∿∿  │ ▌   │
│ 155°    │   75 %     │      │ ▌   │
├────────────────────────────────────┤ Y=320
│  [↺]   [▶]    0:00      [💾]     │ 55px - Controlli
├────────────────────────────────────┤ Y=375
│ [💡] [∿∿∿] [🔥]  [∿∿∿]          │ 105px - 8 pulsanti funzione
│  [P]  [m∿]  [🌿]   [≡]           │
└────────────────────────────────────┘ Y=480
```

**Legenda 4 colonne:**
- COL1: SET cielo (icona + temp), SET platea (icona + temp)
- COL2: Temperatura grande 72pt, Potenza cielo %, linea, Potenza platea %
- COL3: °C, icona platea, temp platea°, resistenza cielo, resistenza platea
- COL4: Barra temperatura verticale 36 segmenti (rosso→giallo→verde)

### Costanti Layout (ui_theme.h)
```cpp
#define HEADER_Y        0       // 35px
#define PHASES_Y        35      // 105px
#define STATUS_Y        130     // 28px
#define TEMP_Y          168     // 90px
#define SETPWR_Y        258     // 62px
#define CONTROLS_Y      320     // 55px
#define BUTTONS_Y       375     // 105px
```

### Funzionalità Touch

| Elemento | Touch | Funzione |
|----------|-------|----------|
| Fasi 1-2-3 | SI | Seleziona fase, aggiorna target |
| Barra stato | NO | Mostra PROG/NOME/STATO |
| Temperatura grande | NO | Display temp cielo |
| Barra grafica | NO | 8 segmenti rosso→giallo→verde |
| SET Cielo | SI | Apre tastierino temperatura |
| SET Platea | SI | Apre tastierino temperatura |
| PWR Cielo | SI | Apre tastierino potenza % |
| PWR Platea | SI | Apre tastierino potenza % |
| Resistenze | NO | Lampeggio rosso quando attive |
| Restart | SI | Azzera timer |
| Play/Pause | SI | Avvia/pausa countdown timer |
| Timer | SI | Apre tastierino minuti |
| Save | SI | (Non implementato) |
| LUCE | SI | Toggle luce camera |
| BOOST CIELO | SI | Toggle boost cielo (countdown) |
| PREHEAT | SI | Attiva/disattiva preriscaldamento |
| BOOST PLATEA | SI | Toggle boost platea (countdown) |
| P (Programmi) | NO | (Non implementato) |
| MAINTAIN | SI | Attiva/disattiva mantenimento |
| POWER | SI | Toggle ON/OFF forno |
| MENU | NO | (Non implementato) |

### Stati del Forno (OvenMode)

| Stato | Condizione | Colore Status Bar | Testo |
|-------|------------|-------------------|-------|
| STANDBY | Forno spento / in attesa | Grigio | Bianco |
| PREHEAT | Preriscaldamento attivo | Arancione | Nero |
| MAINTAIN | Mantenimento temperatura | Giallo | Nero |
| READY | Pronto per cottura | Verde | Nero |
| COOKING | Timer attivo | Bianco | Nero |
| BOOST | Boost cielo/platea attivo | Rosso | Bianco |

### Transizioni Automatiche

- **PREHEAT → MAINTAIN**: Indipendente per cielo/platea, cutoff adattivo dal profilo termico (default: cielo 4C, platea 10C prima del target)
- **MAINTAIN → READY**: Quando entrambi gli elementi sono a temperatura
- **COOKING**: Sticky, esce solo via standby. Funziona con o senza timer
- **BOOST disattivazione**: Automatica allo scadere del countdown (max 180s)
- **Auto-standby**: Dopo 1 ora di inattivita' (se non in cottura)

---

## File di Riferimento

- `documentazione/Effeuno-presenta-2.png` - Screenshot interfaccia Effeuno
- `documentazione/effeuno_libretto_a4_evolution_multi_01_2024.pdf` - Manuale Effeuno
- `documentazione/firmware.md` - Documentazione architettura completa

### Codice Sorgente (v3.25)
```
firmware/oven_controller/src/
├── main.cpp                # Entry point, init e loop
├── lv_conf.h               # Configurazione LVGL
├── hardware_pins.h         # Costanti GPIO e timing
├── display_driver.cpp/h    # LovyanGFX + LVGL callbacks
├── oven_state.cpp/h        # Stato globale + logica modalita'
├── thermal_profile.cpp/h   # Auto-learning profilo termico
├── sensors.cpp/h           # Lettura MAX31855 (2 sonde)
├── ssr_control.cpp/h       # Driver SSR burst firing 100Hz
├── programs_storage.cpp/h  # Persistenza 20 programmi NVS
├── settings_storage.cpp/h  # Persistenza impostazioni NVS
├── wifi_storage.cpp/h      # Persistenza credenziali WiFi NVS
├── buzzer.cpp/h            # Driver buzzer PWM
├── fan.cpp/h               # Ventola PWM + TACH RPM
├── wifi_time.cpp/h         # WiFi + sincronizzazione NTP
├── ota_server.cpp/h        # Server HTTP per OTA update
├── auto_presets.cpp/h      # Preset alimentari (legacy)
├── ui_theme.cpp/h          # Stili e colori Effeuno
├── ui_widgets.cpp/h        # Widget custom (gauge, bar, icons)
├── ui_mode_select.cpp/h    # Selezione modalita' SEMPLICE/PRO
├── ui_main.cpp/h           # Schermata principale PRO (Effeuno)
├── ui_auto.cpp/h           # Schermata principale SEMPLICE (PRO-style)
├── ui_keypad.cpp/h         # Tastierino numerico
├── ui_keyboard.cpp/h       # Tastiera alfanumerica QWERTY
├── ui_settings.cpp/h       # Impostazioni (2 pagine)
├── ui_programs.cpp/h       # Selezione programmi
├── ui_program_edit.cpp/h   # Editor programmi
├── ui_wifi.cpp/h           # Configurazione WiFi
└── ui_ota.cpp/h            # OTA update con progress bar
```

---

## Changelog

### v3.25 (2026-02-10) - Riscrittura Modalita' SEMPLICE PRO-style
- **Riscrittura completa `ui_auto.cpp`** - layout identico al PRO:
  - Selettore programmi con card (nome arancione + info temp/potenza) e frecce P0-P19
  - Status bar identica al PRO (Y=130): `P0 MANUALE` | ETA countdown | STANDBY
  - Sezione temperatura 4 colonne: +5/SET/-5 | temp grande 72pt + potenze | °C/platea/resistenze | barra
  - Controlli identici al PRO: Restart, Play, Timer, Save
  - Bottone azione unico: ACCENDI (verde) / CUOCI (arancione) / SPEGNI (rosso)
  - Riga 2: PROG + MENU
- **+/- temperatura con delta**: cambia cielo e platea insieme mantenendo il delta del programma
- **Potenze read-only**: non cliccabili in modalita' SEMPLICE (vengono dal programma)
- **Big temp mostra max(cielo, platea)** invece di solo cielo
- **Programmi bloccati quando forno acceso** (frecce disabilitate)
- **Save**: `programs_save_current_phase_params()` con feedback bordo verde 1s
- **auto_presets.h non piu' incluso** da ui_auto (legacy, non eliminato)
- **Fix crash `ui_programs_hide()`**: tornava sempre a `ui_main_get_screen()` (nullptr se in SEMPLICE). Ora controlla `ui_auto_is_visible()` e torna allo screen corretto.
- Flash: 72.7%, RAM: 36.2%

### v3.24 (2026-02-09) - Modalita' Automatica con Preset Alimentari
- **Modalita' SEMPLICE (forno casalingo)**: `ui_mode_select`, `ui_auto`, `auto_presets`
- 11 preset alimentari (arrosto, pollo, patate, torta, etc.) con potenze calibrate
- Schermata selezione all'avvio: SEMPLICE / PRO
- Preheat cielo 90% (era 100%) per sincronizzare con platea
- NVS persistence per preset e temperatura auto (settings_storage)
- Bottone "Cambia Modalita'" in Settings > SISTEMA

### v3.23 (2026-02-09) - Ricalibrazione Potenze Programmi
- **Potenze ricalibrate per Vevor 1000W+1000W simmetrico**:
  - Tutti i programmi pizza (P1-P7): cielo 100% (1000W non ha margine)
  - Teglia e Focaccia: platea dominante (75/90, 70/90) per cottura attraverso teglia
  - Gratinatura (lasagne, gratin, bruschetta): cielo forte (80-90/35-45)
  - Dolci (torta, torta salata): platea dominante (45-50/50-65)
  - Carni (arrosto, pollo): cielo aumentato per rosolatura (80-85/55)
- **P4 AMERICANA aggiornato con dati reali testati**: 365/330°C, 100/80, 5min, boost 120s
- **Fix bug PROGRAMS_VERSION**: `programs_storage_init()` non confrontava mai la versione NVS con la costante, il bump di versione non aveva effetto. Ora funziona correttamente.
- **PROGRAMS_VERSION**: 2 → 3 (forza ricreazione default al prossimo boot)
- Flash: 72.3%, RAM: 36.2%

### v3.22 (2026-02-08) - Auto-learning Termico
- **Eliminazione calibrazione manuale** (~30min procedura):
  - Rimossi `calibration.h/cpp`, `ui_calibration.h/cpp`
  - Rimosso bottone "Calibrazione Forno" da Impostazioni
- **Nuovo modulo `thermal_profile.cpp/h`** - auto-learning:
  - Impara rate riscaldamento per 5 bande temperatura (25-100, 100-200, 200-300, 300-400, 400+)
  - Impara overshoot per 3 target range (<250, 250-350, >350)
  - Autotune potenza maintain +-2% ogni 60s, salva quando stabile 2+ min
  - Ring buffer 16 campioni per rate real-time
  - Blend EMA con cap sessioni a 10
  - Default dal forno reale: cielo 7.9C/min, platea 8.6C/min, overshoot 4/10C, maintain 60%
- **Cutoff preheat→maintain adattivo**: basato su overshoot appreso (default: cielo 4C, platea 10C)
- **ETA basato su max(cielo, platea)**: mostra il tempo del peggiore, "PRONTO" solo quando entrambi a target
- **Bottone MAINTAIN acceso anche in READY**
- **Salvataggio NVS nei momenti chiave**: cambio banda, transizione maintain, overshoot, maintain convergato, 5min safety, standby
- **Factory reset** include reset profilo termico
- Flash: 72.3%, RAM: 36.2%

### v3.21 (2026-02-06) - Calibrazione Indipendente e Programmi Cottura
- **Calibrazione indipendente per cielo e platea**:
  - Spegnimento SSR indipendente per elemento in HEATING e MAINTAIN_SEARCH
  - Overshoot tracciato gia' durante HEATING
  - Stabilizzazione maintain indipendente: partenza 10%, cutoff isteresi
- **COOKING mode sticky**: resta dopo timer scaduto, esce solo con standby
- **Funziona anche senza timer** (duration=0 = cottura infinita)
- **Preheat/maintain da calibrazione**: 100% preheat, potenza maintain da dati calibrati
- **Programmi default P1-P18**: 7 pizze + 11 cotture (pane, arrosto, pollo, patate, ecc.)
- **Buzzer alert migliorato**: 5x400ms a 800Hz + pause 250ms = 3.25s totali

### v3.19 (2026-01-19) - Programmi Cottura e Gestione Completa
- **20 programmi (P0-P19)** con persistenza NVS versionata
- **P0 (MANUALE)**: auto-salva da main screen, caricato all'avvio
- **P1-P5 preimpostati**: Napoletana, Romana, Teglia, Americana, Focaccia
- **Schermata selezione programmi**: lista paginata, tap per selezionare
- **Editor programmi**: tabella stile Effeuno con fasi, temperature, potenze, durate
- **Tastiera alfanumerica QWERTY** per nomi programmi
- **Fasi multi-cottura**: fino a 3 fasi con passaggio automatico

### v3.15 (2026-01-18) - OTA Firmware Update e Configurazione WiFi
- **Aggiornamento firmware OTA via browser**:
  - Attivazione manuale da Impostazioni → "Aggiorna Firmware"
  - Web server HTTP su porta 80 con pagina upload
  - Progress bar sul display durante upload
  - Auto-reboot dopo upload completato
- **Configurazione WiFi da interfaccia**:
  - Nuova schermata Impostazioni → WiFi
  - Inserimento SSID e password con tastiera
  - Test connessione con visualizzazione IP
  - Credenziali salvate in NVS (persistenti)
- **Partition scheme modificato**: `min_spiffs.csv` per supporto OTA dual-partition
- **Schermata Impostazioni scrollabile**: aggiunto container scroll per nuovi bottoni
- **Nuovi file**:
  - `wifi_storage.cpp/h` - Persistenza credenziali WiFi in NVS
  - `ota_server.cpp/h` - Web server per upload firmware
  - `ui_wifi.cpp/h` - Schermata configurazione WiFi
  - `ui_ota.cpp/h` - Schermata OTA con progress bar
- **File modificati**:
  - `wifi_time.cpp` - Usa wifi_storage, nuove funzioni keep_connected/reconnect
  - `wifi_config.h` - Credenziali come fallback (non più hardcoded)
  - `ui_settings.cpp` - Bottoni WiFi e Aggiorna FW, scroll container
  - `main.cpp` - Init wifi_storage, update ui_wifi/ui_ota
  - `platformio.ini` - Partition scheme min_spiffs.csv

### v3.14 (2026-01-18) - Riscaldamento Indipendente Cielo/Platea
- **Transizione PREHEAT→MAINTAIN indipendente per resistenza**:
  - Cielo passa a potenza MAINTAIN quando raggiunge il proprio target
  - Platea continua con potenza PREHEAT fino al proprio target
  - Prima: entrambe passavano a MAINTAIN insieme (sbagliato)
- **Modo READY solo quando entrambe a temperatura**:
  - Cielo a target + Platea a target → READY
  - Se solo una a target → MAINTAIN
- **Nuovi flag in OvenState** (`oven_state.h`):
  - `cielo_in_maintain` - true quando cielo ha raggiunto target
  - `platea_in_maintain` - true quando platea ha raggiunto target
- **Logica `get_effective_power` migliorata**:
  - Restituisce potenza preheat/maintain in base allo stato individuale
  - Boost ha sempre priorità

### v3.13 (2026-01-18) - Pulsanti Riga 1 Disabilitati quando OFF
- **Pulsanti riga 1 disabilitati quando power OFF**:
  - PREHEAT, MAINTAIN, BOOST CIELO, BOOST PLATEA
  - Appaiono grigi chiari quando forno spento
  - Non rispondono al touch quando disabilitati
  - Si attivano automaticamente quando si preme ON
- Nuova funzione `ui_widget_set_func_btn_enabled()` per gestire stato enabled/disabled
- Aggiunto controllo `power_on` nei click handler di BOOST_CIELO e BOOST_PLATEA

### v3.12 (2026-01-18) - SSR Control e Quadranti Fasi Colorati
- **Driver SSR implementato** (ssr_control.cpp/h):
  - Burst firing con periodo 1 secondo (100 step da 10ms)
  - Zero-crossing per SSR-40DA
  - GPIO25 cielo, GPIO5 platea (cambiato da GPIO14 per conflitto)
  - Potenza 1000W per resistenza
- **Quadranti fasi con colori distintivi**:
  - Fase 1: giallo
  - Fase 2: arancione
  - Fase 3: rosso
  - Arco che si riempie progressivamente durante cottura
  - Sfondo arco = colore fase "spento" (25%)
- **Status bar COOKING**: colore bordeaux (#800020) invece di bianco
- **Icone resistenze** ora riflettono stato reale SSR (lampeggio in base al burst firing)
- Nuovi file: `ssr_control.cpp`, `ssr_control.h`

### v3.11 (2026-01-17) - Schermata Edit Programmi Stile Tabella Effeuno
- **Layout tabella identico all'originale Effeuno**:
  - Sezione intestazione: Programma, Descrizione, Numero fasi
  - Tabella fasi con colonne Fase 1, Fase 2, Fase 3
  - Righe: Temper.Cielo, Temper.Platea, T.cottura, Pot.Cielo, Pot.Platea
  - Parametri globali: Preriscaldamento, Mantenimento, Boost
- **Stile visivo celle** con sfondo grigio scuro (#1A1A1A etichette, #2A2A2A valori)
- **Colonne fasi dinamiche**: nascoste automaticamente in base a "Numero fasi"
- **Click ciclico numero fasi**: tap per ciclare 1→2→3→1
- **Tutti i valori editabili** con tap su cella
- Rimossa versione precedente con layout a lista scrollabile

### v3.10 (2026-01-17) - Ottimizzazioni UI e Temperatura Box
- **Temperatura box nell'header**: mostra temp ambiente (media cold junction dei 2 MAX31855)
- **Ridotto a 20 programmi** (P0-P19) per ottimizzare memoria
- **Lista programmi paginata**: 10 items/pagina, 2 pagine (rimossa versione scroll)
- **P0 ora editabile** anche dalla schermata Gestione Programmi
- **Fix header padding**: pulsanti back/save non più tagliati (aggiunto pad_all=0)
- **Fix crash gestione memoria LVGL**:
  - Ordine corretto: prima caricare nuova schermata, poi eliminare la vecchia
  - `ui_programs_destroy()` per liberare memoria prima di aprire editor
- Posizione pulsante SALVA corretta (non più troppo a destra)

### v3.9 (2026-01-17) - Modifica Programmi Completa
- **Schermata modifica programmi (ui_program_edit.cpp)** - completa:
  - Header con back e pulsante SALVA verde
  - Sezioni scrollabili: Nome, Fasi, Preheat, Maintain, Boost
  - Modifica nome con tastiera alfanumerica
  - Selezione numero fasi (1-3) con tap ciclico
  - Per ogni fase: temp cielo/platea, durata, potenze
  - Potenze preheat cielo/platea
  - Potenze maintain cielo/platea
  - Tempo boost (0-180 sec)
  - Fasi nascoste automaticamente in base a num_phases
- **Tastiera alfanumerica (ui_keyboard.cpp)**:
  - Layout QWERTY compatto per display 320px
  - Numeri 0-9 sulla prima riga
  - Backspace e conferma OK
  - Counter caratteri (es. "8/16")
  - Cursore lampeggiante
- **Navigazione migliorata**:
  - Keypad torna automaticamente a program_edit dopo modifica
  - Keyboard torna a program_edit dopo modifica nome
- **Fix memoria LVGL**: aumentata da 48KB a 64KB per supportare ~90 oggetti della schermata edit
- **Fix crash su uscita**: invertito ordine operazioni in `ui_program_edit_hide()` (prima carica nuova schermata, poi elimina la vecchia)
- Nuovi file: `ui_program_edit.cpp`, `ui_keyboard.cpp`

### v3.7 (2026-01-17) - Audio e Impostazioni
- **Buzzer passivo su GPIO4** con driver PWM
- Beep feedback su tutti i pulsanti UI e keypad
- Doppio beep conferma quando si imposta un valore
- **Allarme sonoro quando timer scade** (3 toni bassi)
- Tono breve all'avvio del sistema
- **Pagina impostazioni** raggiungibile da pulsante MENU:
  - Toggle suono ON/OFF con switch LVGL
  - Slider volume 10-100%
  - Beep test quando si regola il volume
- **Icona suono nell'header** a destra (volume/mute)
- Abilitati widget LVGL: `LV_USE_SLIDER`, `LV_USE_SWITCH`
- Nuovi file: `buzzer.cpp/h`, `ui_settings.cpp/h`

### v3.6 (2025-01-17) - Stati Pulsanti e Status Bar Dinamica
- **Icone resistenze con caps orizzontali** (zigzag 9 punti + 6px caps)
- Icone PREHEAT: 2 resistenze, MAINTAIN: "m" + resistenza, BOOST: resistenza + "BOOST"
- Icone resistenze sotto platea con caps 5px e linea più sottile
- MENU: cambiato a LV_SYMBOL_LIST (3 linee orizzontali)
- **Stati pulsanti attivi**:
  - PREHEAT: sfondo arancione, icone nere
  - MAINTAIN: sfondo giallo, icone nere
  - BOOST: sfondo rosso, icone nere
  - POWER: sfondo verde, icona nera
  - LIGHT: bordo e icona gialla
- **Status bar colori dinamici**:
  - STANDBY: grigio, PREHEAT: arancione, MAINTAIN: giallo
  - COOKING: bianco, READY: verde, BOOST: rosso
  - Testo nero su sfondi chiari
- **Countdown temperatura** al posto del nome programma (-M:SS o "PRONTO")
- Boost mostra 100% nella visualizzazione potenza
- Bordo bianco per tutti i pulsanti quando premuti
- STATUS_Y spostato a 130 (era 140)

### v3.5 (2025-01-17) - Icone Pulsanti e Nuovo Layout
- **Rimosso sfondo grigio** area pulsanti (ora nero come resto schermo)
- **Nuovo ordine pulsanti**:
  - Riga 1: PREHEAT, MAINTAIN, BOOST CIELO, BOOST PLATEA
  - Riga 2: POWER, LUCE, PROGRAMMI, MENU
- **Icone disegnate con primitive LVGL** (no PNG):
  - PREHEAT: 2 resistenze zigzag
  - MAINTAIN: "m" + resistenza sotto
  - BOOST CIELO: resistenza + "BOOST"
  - BOOST PLATEA: "BOOST" + resistenza
  - Resistenze (sotto temp): zigzag con lv_line
- **Rimossi file icon_images.cpp/h** (non più necessari)
- Flash ridotta da 43% a 41.7%

**TODO prossima sessione:**
- Sistemare icone prima riga (verificare dimensioni/posizioni)
- Menu: tornare a 3 linee orizzontali (LV_SYMBOL_LIST invece di SETTINGS)

### v3.4 (2025-01-15) - Power Button e Rimozione ECO
- **Pulsante Power spostato** dall'header ai pulsanti funzione
- Nuovo layout pulsanti riga 2: [PROG] [MAINTAIN] [POWER] [MENU]
- **Rimossa modalità ECO** (non più necessaria)
- Semplificata logica OvenMode (6 stati invece di 7)
- Rimossi eco_mode, eco_reduction dallo stato
- Pulsante Power verde quando acceso, grigio quando spento

### v3.3 (2025-01-15) - Logica Fasi e Colori Modalità
- **Gestione fasi automatica**:
  - Click su fase carica: temperatura, potenza E timer
  - Passaggio automatico alla fase successiva quando timer scade
  - Fine tutte le fasi → READY (riscaldamento continua)
- **Colori potenze in base alla modalità**:
  - Arancio durante PREHEAT (non editabili)
  - Rosso durante BOOST
  - Bianco normale (editabili)
- **Miglioramenti conformi al manuale Effeuno**:
  - Potenze bloccate durante PREHEAT
  - Pausa timer non ferma riscaldamento
  - Play da MAINTAIN avvia cottura

### v3.2 (2025-01-15) - WiFi & Layout Refinement
- **WiFi/NTP sincronizzazione ora automatica**
- Connessione WiFi all'avvio, disconnessione dopo sync
- Icona WiFi nell'header (lampeggio durante connessione)
- Data/ora visibile solo dopo sincronizzazione NTP
- **Layout 4 colonne** per sezione temperatura/potenza
- Icone SET cielo/platea ridisegnate (rettangolo bianco + linea nera)
- Barra temperatura verticale con 36 segmenti (3x più dettaglio)
- Font temperatura grande 72pt con centratura
- Intervallo lettura sensori aumentato a 2000ms
- Rinominata cartella firmware: test_termocoppia → oven_controller

### v3.1 (2025-01-14) - Effeuno Edition
- **Interfaccia replica esatta Effeuno Evolution Multi**
- Nuovi stati: PREHEAT, MAINTAIN, BOOST, ECO, COOKING
- Potenze separate: normale, preheat, maintain
- Boost con countdown automatico (max 180s)
- Modalità ECO con riduzione 40%
- Luce camera toggle
- Auto-standby dopo 1 ora inattività
- Transizione automatica PREHEAT → MAINTAIN
- Icone SET cielo (3 linee) e platea (1 linea)
- Barra temperatura a 8 segmenti colorati
- Layout e colori esatti dal manuale Effeuno

### v3.0 (2025-01-14) - LVGL Edition
- **Migrazione completa a LVGL 8.4.0**
- Architettura modulare: logica separata da UI
- File strutturati: main.cpp, display_driver, oven_state, sensors, ui_*
- Widget LVGL nativi: lv_meter (gauge), lv_bar, lv_btn, lv_btnmatrix, lv_led
- Tema scuro personalizzato con colori Effeuno
- Tastierino numerico con lv_btnmatrix
- Double buffer DMA per rendering fluido
- RAM: 22.8%, Flash: 25.2%
- Touch gestito da LVGL (debounce automatico)

### v2.0 (2025-01-12)
- Riscrittura completa interfaccia stile Effeuno
- Indicatori fasi circolari con tacche (tachimetro)
- Temperatura gigante centrale con barra grafica colorata
- SET/PWR in linea per cielo e platea
- 8 pulsanti funzione su 2 righe con icone
- Animazione resistenze zigzag lampeggianti
- Zone touch conformi standard 44px minimo
- Timer countdown con lampeggio barra stato

### v1.0
- Interfaccia base con 2 zone separate cielo/platea
- Box SET e PWR verticali
- Supporto 2 sonde MAX31855

---

## Prossimi Passi

### Completato
- [x] Controllo SSR (v3.12)
- [x] OTA + WiFi (v3.15)
- [x] Ventola TACH (v3.18)
- [x] Programmi cottura (v3.19-v3.21)
- [x] Auto-learning termico (v3.22)

### Da fare
1. **Schermata Log Allarmi** - Storico errori e eventi
2. **Import/Export USB** - File PROGRAMS.CSV come Effeuno

### Layout pulsanti attuale (v3.6)
```
Riga 1: [PREHEAT] [MAINTAIN] [BOOST↑] [BOOST↓]
Riga 2: [POWER]   [LUCE]     [P]      [MENU]
```

**Stati pulsanti:**
- Disabilitato (power OFF): sfondo grigio scuro, icona grigia, non cliccabile (solo riga 1)
- Inattivo: bordo grigio, icona bianca
- Attivo: sfondo colorato, icona nera (PREHEAT=arancione, MAINTAIN=giallo, BOOST=rosso, POWER=verde)
- Premuto: bordo bianco
