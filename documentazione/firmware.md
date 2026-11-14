# Firmware Forno Digitale

## Panoramica

Firmware per il controllo di un forno elettrico convertito da analogico a digitale.
Interfaccia replica esatta del forno **Effeuno Evolution Multi**.

- **Hardware**: WT32-SC01 (ESP32 con display touch 480x320)
- **Sensore temperatura**: 2x MAX31855 con termocoppia tipo K
- **Framework**: Arduino + PlatformIO
- **Libreria display**: LovyanGFX (driver) + **LVGL 8.4.0** (UI)
- **Versione attuale**: 3.25 Modalita' SEMPLICE PRO-style + fix navigazione

## Struttura progetto (v3.25)

```
firmware/oven_controller/
├── platformio.ini              # Configurazione PlatformIO + LVGL
└── src/
    ├── main.cpp                # Entry point, init e loop
    ├── lv_conf.h               # Configurazione LVGL
    ├── hardware_pins.h         # Costanti GPIO e timing
    ├── display_driver.cpp/h    # LovyanGFX + callback LVGL
    ├── sensors.cpp/h           # Wrapper MAX31855 (2 sonde)
    ├── ssr_control.cpp/h       # Driver SSR con burst firing 100Hz
    ├── oven_state.cpp/h        # Stato forno centralizzato + logica modalita'
    ├── thermal_profile.cpp/h   # Auto-learning profilo termico (NVS)
    ├── programs_storage.cpp/h  # Persistenza 20 programmi (NVS)
    ├── settings_storage.cpp/h  # Persistenza impostazioni audio (NVS)
    ├── wifi_storage.cpp/h      # Persistenza credenziali WiFi (NVS)
    ├── buzzer.cpp/h            # Driver buzzer passivo PWM
    ├── fan.cpp/h               # Ventola PWM + TACH RPM
    ├── wifi_time.cpp/h         # WiFi + sincronizzazione NTP
    ├── ota_server.cpp/h        # Server HTTP per OTA update
    ├── ui_theme.cpp/h          # Tema scuro Effeuno
    ├── ui_widgets.cpp/h        # Widget custom (gauge, barra, icone)
    ├── ui_mode_select.cpp/h    # Selezione modalita' avvio (SEMPLICE/PRO)
    ├── ui_main.cpp/h           # Schermata principale PRO (Effeuno style)
    ├── ui_auto.cpp/h           # Schermata principale SEMPLICE (PRO layout + selettore programmi)
    ├── auto_presets.cpp/h      # Preset alimentari (legacy, non usato da ui_auto v3.25)
    ├── ui_keypad.cpp/h         # Tastierino numerico
    ├── ui_keyboard.cpp/h       # Tastiera alfanumerica QWERTY
    ├── ui_settings.cpp/h       # Schermata impostazioni (2 pagine)
    ├── ui_programs.cpp/h       # Schermata selezione programmi
    ├── ui_program_edit.cpp/h   # Schermata modifica programmi
    ├── ui_wifi.cpp/h           # Schermata configurazione WiFi
    └── ui_ota.cpp/h            # Schermata OTA con progress bar
```

## Architettura LVGL

### Flusso Dati

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  sensors.cpp │────▶│ oven_state  │────▶│  ui_main    │
│  (MAX31855)  │     │  (globale)  │     │  (LVGL)     │
└─────────────┘     └─────────────┘     └─────────────┘
                           │
                    ┌──────┴──────┐
                    ▼             ▼
              ui_keypad      ui_widgets
              (input)        (gauge, bar, icons)
```

### File Principali

| File | Responsabilita' |
|------|----------------|
| `main.cpp` | Init LVGL, loop con `lv_timer_handler()` |
| `display_driver.cpp` | Configura LovyanGFX come backend LVGL |
| `oven_state.cpp` | Struct `g_state` con tutte le variabili + logica modalita' |
| `thermal_profile.cpp` | Auto-learning profilo termico (rate, overshoot, maintain) |
| `sensors.cpp` | Lettura MAX31855, aggiorna `g_state` |
| `programs_storage.cpp` | Persistenza 20 programmi in NVS |
| `ssr_control.cpp` | Burst firing SSR 100Hz con override mode |
| `ui_mode_select.cpp` | Selezione SEMPLICE/PRO all'avvio |
| `ui_main.cpp` | Layout PRO Effeuno completo, callback eventi |
| `ui_auto.cpp` | Layout SEMPLICE: PRO + selettore programmi + azione unica |
| `ui_keypad.cpp` | Overlay tastierino per input numerico |
| `ui_settings.cpp` | Impostazioni: audio, ventola, sistema |
| `ui_theme.cpp` | Stili e colori (tema scuro Effeuno) |
| `ui_widgets.cpp` | Widget: gauge fasi, barra temp, icone resistenze |

### Widget LVGL Utilizzati

| Elemento UI | Widget LVGL |
|-------------|-------------|
| Gauge fasi (3x) | `lv_arc` (270° con indicatore) |
| Barra temperatura | Custom (8 segmenti colorati) |
| Pulsanti funzione | `lv_btn` (2x4 grid) |
| Pulsanti controllo | `lv_btn` (restart, play, save) |
| Tastierino | `lv_btnmatrix` |
| Icone SET | Custom (linee orizzontali) |
| Icone resistenze | `lv_label` con simboli |
| Etichette | `lv_label` |

### Timing

- **LVGL handler**: ogni 5ms (nel loop)
- **SSR control**: ogni 10ms (burst firing 100Hz)
- **Lettura sensori**: ogni 2000ms
- **Aggiornamento UI**: ogni 100ms
- **Animazione resistenze**: riflette stato reale SSR
- **Clock**: ogni 1000ms
- **Boost countdown**: ogni 1000ms
- **Auto-standby check**: ogni loop

## Interfaccia Utente (Effeuno Style)

### Layout (320x480 Portrait)

```
┌────────────────────────────────────┐  0
│ MARTEDI' 07.02.2023 - 12:52  [PWR] │ 35px - Header
├────────────────────────────────────┤ 35
│    155°       156°       157°      │
│   ⌒2:00⌒    ⌒2:00⌒    ⌒2:00⌒    │ 105px - 3 Fasi circolari
│    155°       156°       157°      │
├────────────────────────────────────┤ 140
│ PROG. 5 │ EFFEUNO P5 │   STANDBY  │ 28px - Status bar
├────────────────────────────────────┤ 168
│ SET                                │
│ ≡≡≡   104    °C    ▌▌▌▌          │ 90px - Temp grande
│ 155°              47°              │       + barra segmentata
├────────────────────────────────────┤ 258
│ SET                                │
│ ▄    75 %           ∿∿∿          │ 62px - Potenze + resistenze
│ 155° 75 %           ∿∿∿          │
├────────────────────────────────────┤ 320
│  [↺]   [▶]    0:00      [💾]     │ 55px - Controlli
├────────────────────────────────────┤ 375
│ [💡] [∿∿∿] [🔥]  [∿∿∿]         │
│  [P]  [m∿]  [🌿]   [≡]           │ 105px - 8 pulsanti funzione
└────────────────────────────────────┘ 480
```

### Costanti Layout (ui_theme.h)

```cpp
#define HEADER_Y        0       // 35px
#define PHASES_Y        35      // 105px
#define STATUS_Y        140     // 28px
#define TEMP_Y          168     // 90px
#define SETPWR_Y        258     // 62px
#define CONTROLS_Y      320     // 55px
#define BUTTONS_Y       375     // 105px
```

### Riferimento Grafico

L'interfaccia replica esattamente lo stile **Effeuno Evolution Multi**.
Immagine di riferimento: `documentazione/Effeuno-presenta-2.png`
Manuale di riferimento: `documentazione/effeuno_libretto_a4_evolution_multi_01_2024.pdf`

## Stati del Forno (OvenMode)

| Stato | Descrizione | Colore Status Bar |
|-------|-------------|-------------------|
| `STANDBY` | Forno in attesa di comandi | Nero (sfondo) |
| `PREHEAT` | Preriscaldamento camera | Arancione |
| `MAINTAIN` | Mantenimento temperatura | Nero |
| `READY` | Pronto per cottura | Verde |
| `COOKING` | Timer cottura attivo (sticky) | Bordeaux |
| `BOOST` | Boost cielo/platea attivo | Rosso |

### Transizioni Automatiche

```
STANDBY ──[Preheat]──▶ PREHEAT ──[cutoff adattivo]──▶ MAINTAIN ──[entrambi OK]──▶ READY
                                                                                    │
                                                                              [Play]▼
                                                                               COOKING
```

- **PREHEAT → MAINTAIN**: Indipendente per cielo/platea. Cutoff adattivo dal profilo termico (default: cielo 4C, platea 10C prima del target)
- **MAINTAIN → READY**: Quando entrambi gli elementi sono a temperatura
- **COOKING**: Sticky, esce solo via standby. Funziona con o senza timer
- **Dopo timer scaduto**: resta in COOKING, mantiene temperatura/potenza ultima fase
- **Auto-standby**: Dopo 1 ora di inattivita' (se non in cottura)

### Auto-learning Profilo Termico (v3.22)

Sostituisce la calibrazione manuale. Impara dal funzionamento normale.

**Default dal forno reale** (usati fino a quando dati appresi li sostituiscono):

| Parametro | Cielo | Platea |
|-----------|-------|--------|
| Rate riscaldamento | 7.9 C/min | 8.6 C/min |
| Overshoot | 4 C | 10 C |
| Maintain power | 60% | 60% |

**5 bande temperatura**: 25-100, 100-200, 200-300, 300-400, 400+ C
**3 target range**: <250, 250-350, >350 C

**Funzionamento** (`thermal_profile_record()`, ogni 2s):
1. Ring buffer 16 campioni per rate real-time
2. PREHEAT: accumula rate per banda, salva NVS al cambio banda
3. Transizione a maintain: traccia overshoot (3 cali = confermato), blend EMA
4. MAINTAIN/READY: autotune potenza +-2% ogni 60s, salva quando stabile 2+ min
5. Transizione a STANDBY: salvataggio finale

**ETA**: basato sul max tra cielo e platea (chi ci mette di piu'). "PRONTO" solo quando entrambi a target.

**Salvataggio NVS**: completamento banda, preheat→maintain, overshoot confermato, maintain convergato, ogni 5min safety, standby

## Funzionalita' Implementate

### Interfaccia Effeuno Completa (v3.1+)
- [x] Layout esatto come manuale Effeuno Evolution Multi
- [x] 3 indicatori fasi circolari (arc 270°)
- [x] Temperatura grande centrale (72pt)
- [x] Barra temperatura segmentata (rosso→giallo→verde)
- [x] Icone SET cielo/platea e resistenze con lampeggio
- [x] 8 pulsanti funzione (2x4 grid) con stati colorati
- [x] Pulsanti controllo: Restart, Play/Pause, Timer, Save
- [x] Status bar con PROG/NOME/STATO e colori dinamici

### Modalita' Operative (v3.14+)
- [x] **PREHEAT**: 100% potenza, transizione indipendente per cielo/platea
- [x] **MAINTAIN**: Potenza da profilo termico (auto-learned)
- [x] **READY**: Entrambi a temperatura, bottone maintain acceso
- [x] **COOKING**: Sticky, multi-fase, con o senza timer
- [x] **BOOST**: Cielo e/o platea con countdown (max 180s)

### Profilo Termico Auto-learning (v3.22)
- [x] Impara rate riscaldamento per 5 bande temperatura
- [x] Impara overshoot per 3 target range
- [x] Autotune potenza maintain (+-2% ogni 60s)
- [x] Cutoff preheat→maintain adattivo
- [x] ETA per-banda con rate real-time (max cielo/platea)
- [x] Salvataggio NVS nei momenti chiave

### Programmi (v3.21, potenze v3.23)
- [x] 20 programmi (P0-P19) con persistenza NVS
- [x] P0 (MANUALE) auto-salva da main screen
- [x] P1-P18 programmi default (7 pizze + 11 cotture)
- [x] Editor programmi con tabella stile Effeuno
- [x] Tastiera alfanumerica QWERTY per nomi
- [x] Potenze ricalibrate per Vevor 1000W+1000W simmetrico (v3.23)
- [x] Fix PROGRAMS_VERSION check in init (v3.23)

### Connettivita' (v3.15+)
- [x] WiFi + sincronizzazione NTP
- [x] OTA firmware update via browser
- [x] Credenziali WiFi salvate in NVS
- [x] Interfaccia web Gestione Programmi redesign (v3.25):
  - Categorie colorate (ambra MANUALE, rosso pizza, viola cotture, grigio vuoto)
  - Tab filtro sticky (TUTTI / PIZZA / COTTURE)
  - Dot temperatura colorato per range (blu/giallo/arancione/rosso)
  - Transizioni smooth CSS (max-height), rendering incrementale
  - Phase pills, input numerici con min/max/step e inputmode="numeric"
  - Toast notification, save con stati, reset two-step con conferma
  - Validazione client-side (range temp e potenza)

### Audio e Impostazioni (v3.7+)
- [x] Buzzer passivo PWM con volume regolabile
- [x] Allarme timer (5x400ms a 800Hz)
- [x] Melodia avvio (triade C5-E5-G5)
- [x] Settings 2 pagine: Audio+Ventola / Sistema

### Sicurezza
- [x] Ventola PWM con monitoraggio TACH RPM (disabilitabile)
- [x] Auto-standby dopo 1h inattivita'
- [x] Blocco resistenze su errore ventola

### Da Implementare
- [ ] Schermata Log Allarmi
- [ ] Import/Export programmi USB (CSV)
- [ ] Icone grafiche custom

## Collegamento Hardware

### MAX31855 (2 sonde)

| Segnale | GPIO | Sonda |
|---------|------|-------|
| CLK | 33 | Condiviso |
| DO | 32 | Condiviso |
| CS CIELO | 26 | Superiore |
| CS PLATEA | 27 | Inferiore |
| 3V3, GND | - | Alimentazione |

### Display ST7796 (SPI)

| Segnale | GPIO |
|---------|------|
| SCLK | 14 |
| MOSI | 13 |
| DC | 21 |
| CS | 15 |
| RST | 22 |
| BL | 23 (PWM) |

### Touch FT5x06 (I2C)

| Segnale | GPIO |
|---------|------|
| SDA | 18 |
| SCL | 19 |
| INT | 39 |

### SSR Resistenze (SSR-40DA)

| SSR | Resistenza | GPIO | Potenza |
|-----|------------|------|---------|
| #1 | CIELO | GPIO12 | 1000W |
| #2 | PLATEA | GPIO2 | 1000W |

**Controllo**: Burst firing con periodo 1 secondo (100 step da 10ms).
La percentuale di potenza determina quanti step l'SSR è ON.

### Connettori Box Esterno

Il box controller si collega al forno tramite 4 connettori:

| # | Funzione | Poli | Dettaglio |
|---|----------|------|-----------|
| 1 | Alimentazione box | 3 | L (fase) + N (neutro) + PE (terra) |
| 2 | Sonda CIELO | 2 | Termocoppia tipo K (Chromel +, Alumel -) |
| 3 | Sonda PLATEA | 2 | Termocoppia tipo K (Chromel +, Alumel -) |
| 4 | SSR resistenze | 3 | Fase IN comune + Fase CIELO (da SSR) + Fase PLATEA (da SSR) |

**Connettore 1 - Alimentazione (3 poli):**
Porta 230V dal forno al box per alimentare il trasformatore interno (230V → 5V/12V).

**Connettori 2 e 3 - Sonde termocoppia (2 poli ciascuno):**
Collegano le termocoppie tipo K nel forno ai moduli MAX31855 nel box.

**Connettore 4 - SSR (3 poli, rating 17.5A):**

```
Pin 1 (terra): Fase IN (comune)     → 8.7A max (2000W / 230V)  [cavo dal forno]
Pin 2:         Fase CIELO (da SSR1) → 4.35A (1000W / 230V)      [cavo nero]
Pin 3:         Fase PLATEA (da SSR2) → 4.35A (1000W / 230V)     [cavo grigio]
```

**Nota sullo spinotto:** il pin della fase comune IN è collegato al simbolo terra dello spinotto (per distinguerlo dalle uscite SSR).

La fase entra nel box su un pin comune, viene sdoppiata ai due SSR, e ritorna al forno su due pin separati (uno per ogni resistenza). Il neutro delle resistenze resta collegato direttamente nel forno.

**Colori cavi lato SSR (uscita verso forno):**
| Funzione | Colore cavo |
|----------|-------------|
| Fase comune IN (spinotto: simbolo terra) | dal forno |
| Fase CIELO (da SSR1) | nero |
| Fase PLATEA (da SSR2) | grigio |

**Sezione cavi:** 1.5mm² (H05VV-F) - adeguato per 8.7A max, portata nominale 16A.

### Ventola Raffreddamento Box (4-pin PWM)

Ventola 12V 4-pin standard PC per il raffreddamento del box controller.

| Filo ventola | Funzione | Collegamento |
|--------------|----------|--------------|
| Giallo | +12V | Alimentatore esterno 12V |
| Nero | GND | Comune con WT32-SC01 |
| Blu | PWM | GPIO0, facoltativo (non collegato: ventola a 12V fissi; GPIO25 non funziona su questo SC01) |
| Verde | TACH | GPIO5 (pull-up interno ESP32) |

**Controllo PWM**: 25kHz, LEDC channel 2, 8-bit (0-255).
Velocita automatica basata su temperatura box: 25% idle sotto 35°C, rampa lineare fino a 100% a 55°C.
Calibrazione manuale disponibile in Settings.

### Monitoraggio RPM Ventola (TACH)

Il segnale TACH della ventola (2 impulsi/giro) viene letto tramite interrupt su GPIO5 per monitorare gli RPM in tempo reale.

**Parametri di sicurezza** (`hardware_pins.h`):

| Parametro | Valore | Descrizione |
|-----------|--------|-------------|
| `FAN_TACH_PIN` | GPIO5 | Ingresso TACH (pull-up interno) |
| `FAN_TACH_INTERVAL` | 2000ms | Intervallo calcolo RPM |
| `FAN_RPM_MIN_THRESHOLD` | 200 RPM | Sotto questa soglia = ventola ferma |
| `FAN_PWM_MIN_FOR_CHECK` | 30 (~12%) | PWM minimo per attivare il monitoraggio |
| `FAN_FAILURE_COUNT` | 5 letture | Letture consecutive sotto soglia prima di errore |
| `FAN_STARTUP_GRACE_MS` | 4000ms | Tempo di grazia dopo cambio velocita (spinup) |
| `FAN_PULSES_PER_REV` | 2 | Impulsi per giro (standard ventole PC) |

**Calcolo RPM**: `RPM = impulsi * 60000 / (PULSES_PER_REV * INTERVAL)`

**Sequenza di sicurezza** (guasto ventola):

```
Ventola comandata (PWM > 30) ma RPM < 200 per 5 letture consecutive (~10 secondi)
    |
    v
error_fan = true, power_on = false, SSR spenti, STANDBY, buzzer errore
    |
    v
Status bar mostra "FAN ERR" su sfondo rosso
    |
    v
Utente preme POWER ON → errore resettato, forno pronto a ripartire
```

**Blocco resistenze**: quando `error_fan = true`, sia `oven_state_resistance_cielo_on()` che `oven_state_resistance_platea_on()` restituiscono `false`, impedendo qualsiasi attivazione SSR.

**UI**:
- **Schermata principale**: status bar rossa con testo "FAN ERR"
- **Schermata Settings**: label "RPM: N" accanto al titolo VENTOLA (verde = OK, grigio = fermo, rosso = errore)

**File coinvolti**:

| File | Modifiche |
|------|-----------|
| `hardware_pins.h` | Costanti TACH |
| `oven_state.h/cpp` | Campi `error_fan`, `fan_rpm` + blocco resistenze |
| `fan.h/cpp` | ISR, calcolo RPM, rilevamento guasto |
| `main.cpp` | Integrazione nel loop sensori |
| `ui_main.cpp` | "FAN ERR" in status bar + reset errore su POWER ON |
| `ui_settings.cpp/h` | Label RPM live + `ui_settings_update()` |

## Compilazione e Upload

```bash
cd firmware/test_termocoppia

# Compilare
pio run

# Upload su dispositivo
pio run -t upload

# Monitor seriale
pio device monitor
```

## Note Tecniche

### Colori Effeuno (ui_theme.h)

```cpp
#define COLOR_BG          lv_color_hex(0x000000)  // Nero
#define COLOR_HEADER_BG   lv_color_hex(0x1A1A1A)  // Grigio scuro
#define COLOR_GREEN       lv_color_hex(0x00FF00)  // Verde (READY)
#define COLOR_RED         lv_color_hex(0xFF0000)  // Rosso (BOOST)
#define COLOR_ORANGE      lv_color_hex(0xFF8C00)  // Arancione (PREHEAT)
#define COLOR_YELLOW      lv_color_hex(0xFFFF00)  // Giallo (timer)
```

### Buffer Display

Double buffer in DMA, 320x40 righe per buffer:
```cpp
buf1 = heap_caps_malloc(320 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
buf2 = heap_caps_malloc(320 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
```

### Stato Globale (OvenState)

```cpp
struct OvenState {
    // Power & Mode
    bool power_on;
    OvenMode mode;
    bool light_on;

    // Temperatures
    int temp_cielo, temp_platea;
    int target_cielo, target_platea;

    // Power levels
    int power_cielo, power_platea;
    int preheat_power_cielo, preheat_power_platea;
    int maintain_power_cielo, maintain_power_platea;

    // Boost
    bool boost_cielo, boost_platea;
    int boost_time_cielo, boost_time_platea;
    int boost_max_time;  // max 180 sec

    // ECO
    bool eco_mode;
    int eco_reduction;  // default 40%

    // Timer
    int timer_minutes, timer_seconds;
    bool timer_active, timer_paused, timer_expired;

    // Program
    int program_number;
    Phase phases[3];
    String program_name;

    // ...
};
```

## Cronologia Versioni

| Versione | Data | Note |
|----------|------|------|
| 1.0 | 2024 | Test lettura singola termocoppia |
| 2.0 | Gen 2025 | Interfaccia LovyanGFX diretta |
| 3.0 | Gen 2025 | Migrazione a LVGL 8.4.0 |
| 3.1 | Gen 2025 | Interfaccia Effeuno completa |
| 3.11 | Gen 2026 | Schermata edit programmi stile tabella Effeuno |
| 3.12 | Gen 2026 | SSR control, quadranti fasi colorati, status bar bordeaux |
| 3.15 | Gen 2026 | OTA firmware update e configurazione WiFi |
| 3.16 | Gen 2026 | Ventola PWM box cooling |
| 3.17 | Gen 2026 | Rebranding PYRO, calibrazione ventola, suono avvio |
| 3.18 | Feb 2026 | Monitoraggio RPM ventola TACH e sicurezza guasto fan |
| 3.19 | Feb 2026 | Programmi cottura P0-P19, schermata edit, tastiera QWERTY |
| 3.21 | Feb 2026 | Calibrazione indipendente, COOKING sticky, programmi default |
| 3.22 | Feb 2026 | Auto-learning termico, elimina calibrazione manuale |
| 3.23 | Feb 2026 | Potenze ricalibrate per Vevor 1000W+1000W, fix PROGRAMS_VERSION |
| 3.24 | Feb 2026 | Modalita' automatica con preset alimentari, preheat proporzionale |
| **3.25** | **Feb 2026** | **Riscrittura SEMPLICE PRO-style, fix crash navigazione, redesign web programmi** |

## Prossimi Passi

1. **Schermata Log Allarmi** - Storico errori e eventi
2. **Import/Export USB** - File PROGRAMS.CSV
3. **Icone grafiche custom** - Sostituire simboli LVGL

Vedi `TODO-interfaccia-professionale.md` per dettagli completi.
