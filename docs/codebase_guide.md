# Empathy Band — Codebase, Board Config & Activation Guide

Companion to `architecture.md` (design) and `connection_diagram.md` (physical wiring).
This document explains **how the code is organised**, **how the board is configured**, and
**exactly when the system changes state or fires an alert**.

---

## 1. Repository layout

```
iot-project/
├── architecture.md              design spec (problem statement, thresholds, risks)
├── docs/
│   ├── connection_diagram.md    physical wiring (what connects to what)
│   └── codebase_guide.md        this file
├── empathy_band/                FULL firmware (all sensors)
├── empathy_band_min/            MINIMAL firmware (GSR + heart rate + buzzer + LED)
├── esp-idf/                     archived ESP-IDF version (not used for Arduino)
└── dashboard/                   caregiver web dashboard (single index.html)
```

There are **two Arduino sketches**. They share the same classifier and most drivers; the
minimal one simply omits the sensors you don't have wired.

---

## 2. Full vs minimal firmware

| | `empathy_band` (full) | `empathy_band_min` (minimal) |
|---|---|---|
| Sensors | GSR, sound, MPU6050, MAX30102 | GSR + MAX30102 only |
| Outputs | RGB LED, buzzer, vibration motor | RGB LED + buzzer |
| Inputs | push button (caregiver mark) | none |
| Motion gate | real IMU | stubbed to 0 (always "still") |
| Files | gsr, sound, mpu6050, max30102, actuators, button-in-main | gsr, max30102, actuators |

Use the full sketch once everything is wired. Use `empathy_band_min` while you are short on
jumpers — it still reaches every state with the two sensors it has.

---

## 3. Codebase explanation

### 3.1 `config.h` — single source of truth for tunables
Every pin number, I2C address, timing period and threshold lives here. Nothing else hard-codes
a number. Change behaviour by editing this file only.

Key groups:
- **Pins**: `PIN_GSR`, `PIN_RGB_R/G/B`, `PIN_BUZZER`, `PIN_I2C_SDA/SCL`, etc.
- **I2C**: `I2C_ADDR_MPU6050` (0x68), `I2C_ADDR_MAX30102` (0x57), `I2C_FREQ_HZ`.
- **Timing**: `SENSOR_PERIOD_MS` (100), `ACTUATOR_PERIOD_MS` (500).
- **Classification thresholds**: `GSR_LEVEL_THRESH`, `GSR_SLOPE_THRESH`,
  `RMSSD_LOW_THRESH_MS`, `SOUND_THRESH`, `SOUND_SUSTAIN_MS`, `MOTION_STILL_THRESH`,
  `DISTRESS_MIN_POINTS`.
- **Alert timing**: `ALERT_DELAY_MS` (2500).
- **Polarity**: `BUZZER_ACTIVE_LOW`, `GSR_STRESS_RAISES_OUTPUT`.

### 3.2 `band_types.h` — shared data types
- `enum BandState { STATE_CALM, STATE_ANXIOUS, STATE_DISTRESS }` — the three outputs.
- `struct BandInputs` — the normalised feature vector fed to the classifier:
  `gsrLevel`, `gsrSlope`, `soundLevel`, `soundSustained`, `motionMagnitude`, `bpm`,
  `rmssdMs`, `beatValid`.

### 3.3 `classify.{h,cpp}` — the decision core (pure function)
Takes `BandInputs`, returns a `BandState`. It performs **no I/O**, so it is deterministic and
can be unit-tested on a PC (it is: see the host test in the project history). All thresholds
come from `config.h`. Logic is described in section 6.

### 3.4 `i2c_bus.{h,cpp}` — shared I2C helpers
`i2cBegin()` starts `Wire` on GPIO21/22 at 400 kHz. `i2cWriteReg()` / `i2cReadRegs()` are the
two primitives every I2C driver uses. Both sensors share the bus; addresses differ.

### 3.5 `gsr.{h,cpp}` — skin conductance (analog)
- `gsrInit()` configures the ADC pin.
- For the first **30 s** it averages raw counts to learn a **rest baseline**
  (`GSR_CAL_SAMPLES` samples). `gsrCalibrated()` reports when done.
- After calibration it maps the deviation from baseline into a **0..1 level** using
  `GSR_SPAN_COUNTS`, clamped, with polarity set by `GSR_STRESS_RAISES_OUTPUT`.
- `gsrPoll()` returns `{raw, norm}` each call.
- **Slope** is not computed here — the main loop differentiates the level over time.

### 3.6 `sound.{h,cpp}` — vocalisation / ambient noise (analog, full sketch only)
Takes a burst of `SOUND_BURST_SAMPLES` ADC reads and uses **peak-to-peak** amplitude, scaled
from `SOUND_FLOOR_COUNTS`..`SOUND_CEIL_COUNTS` into 0..1. The "sustained" part is decided in
the main loop (a timer), not here.

### 3.7 `mpu6050.{h,cpp}` — motion (I2C, full sketch only)
Reads the accelerometer, converts to g, and reports `motion = || a || - 1g` (deviation from
rest). `mpu6050Present()` lets the main loop skip it if it didn't answer at boot.

### 3.8 `max30102.{h,cpp}` — heart rate + HRV (I2C)
- `max30102Init()` resets the chip, sets SpO2 mode, FIFO and LED currents, clears the FIFO.
- `max30102Poll()` drains the FIFO (red + IR samples).
- A **simplified beat detector** tracks the IR signal's DC with an EMA, detects rising
  zero-crossings as beats, enforces a `300 ms` refractory period, and stores the last 8 R-R
  intervals.
- From those it derives **BPM** (60 000 / mean R-R) and a crude **RMSSD** (HRV).
  `beatValid` is true once enough beats are collected.

### 3.9 `actuators.{h,cpp}` — outputs
- `actuatorsInit()` sets up the buzzer GPIO and three PWM channels for the **common-cathode
  RGB LED** (`ledc`, 5 kHz, 8-bit). Works on both Arduino-ESP32 core 2.x and 3.x.
- `actuatorsUpdate(state, nowMs)` maps state → colour and runs the **sustained-distress
  timer** that gates the buzzer (and motor in the full sketch). It is called every 500 ms.

### 3.10 `<sketch>.ino` — the scheduler
`setup()` initialises everything once. `loop()` is a non-blocking scheduler:
- every **100 ms** → `sensorStep()`: poll inputs, compute GSR slope, sustain timers, build
  `BandInputs`, run `classify()`, store `s_state`, print one log line.
- every **500 ms** → `actuatorsUpdate(s_state, now)`.
- (full sketch) poll the button flag and log caregiver marks.

No `delay()` in the loop, so timing stays stable.

---

## 4. Board configuration (Arduino IDE)

Install the **esp32** board package (Boards Manager URL:
`https://espressif.github.io/arduino-esp32/package_esp32_index.json`). Then:

| Setting | Value |
|---|---|
| Board | **ESP32 Dev Module** |
| Upload Speed | 921600 |
| Flash Size | 4 MB (default) |
| PSRAM | Disabled |
| CPU Frequency | 240 MHz |
| Port | your `/dev/ttyUSB*` or `/dev/ttyACM*` |
| Monitor baud | **115200** |

**No external libraries are required** — I2C uses `Wire` (bundled), the RGB LED uses the
built-in `ledc` PWM, and there is no NeoPixel dependency.

The board's **own USB-C** must be a data cable (some are charge-only). The ESP32 does **not**
sit on the breadboard; only 3V3 and GND are bused to the rails.

---

## 5. Pin diagram & connections

```
                       ┌─────────────────────────────┐
                       │        ESP32 DevKit         │
                       │         (USB-C)             │
                       │                             │
   GSR  SIG ───────────┤ GPIO34   (ADC1_CH6)         │
   Sound AOUT ─────────┤ GPIO35   (ADC1_CH7)  full   │
   Button  A ──────────┤ GPIO4    (pull-up)   full   │
   RGB  R ─────────────┤ GPIO25   (PWM)              │
   RGB  G ─────────────┤ GPIO26   (PWM)              │
   RGB  B ─────────────┤ GPIO27   (PWM)              │
   Vibration ──────────┤ GPIO32   (via NPN)   full   │
   Buzzer  I/O ────────┤ GPIO33                      │
                       │                             │
   I2C SDA ────────────┤ GPIO21 ──┬─ MPU6050 SDA (full)
                       │          └─ MAX30102 SDA
   I2C SCL ────────────┤ GPIO22 ──┬─ MPU6050 SCL (full)
                       │          └─ MAX30102 SCL
                       │                             │
                       3V3 ─────── + breadboard rail │
                       GND ─────── − breadboard rail │
                       └─────────────────────────────┘
```

**Full pin table**

| Function | GPIO | Interface | Sketch |
|---|---|---|---|
| I2C SDA | 21 | I2C | both |
| I2C SCL | 22 | I2C | both |
| GSR analog | 34 | ADC1_CH6 | both |
| Sound analog | 35 | ADC1_CH7 | full |
| Button | 4 | digital in (pull-up) | full |
| RGB red | 25 | PWM | both |
| RGB green | 26 | PWM | both |
| RGB blue | 27 | PWM | both |
| Vibration motor | 32 | digital out (transistor) | full |
| Buzzer | 33 | digital out | both |
| 3V3 / GND | — | power rail | both |

Physical wiring detail (rail taps, transistor circuit, module pinouts) is in
`connection_diagram.md`. Summary of the **minimal** build's wiring:

| Wire | From | To |
|---|---|---|
| power | ESP32 3V3 | + rail |
| power | ESP32 GND | − rail |
| GSR | VCC/GND/SIG | +rail / −rail / GPIO34 |
| MAX30102 | VIN/GND/SDA/SCL | +rail / −rail / GPIO21 / GPIO22 |
| RGB | R/G/B/− | GPIO25 / GPIO26 / GPIO27 / −rail |
| Buzzer | VCC/GND/I/O | +rail / −rail / GPIO33 |

---

## 6. Activation conditions (when the system reacts)

### 6.1 Power-on sequence
1. Serial starts, banner prints.
2. I2C bus starts; sensors initialise.
3. **GSR calibration runs for 30 s** — state is forced CALM and the log prints
   `calibrating GSR rest baseline N%`. **The classifier is effectively inactive until this
   finishes.** (If GSR is present, you must wait it out; the minimal build still calibrates.)
4. After calibration the system **monitors continuously** and updates every 100 ms.

### 6.2 How inputs become points (`classify()`)
Each satisfied condition adds **one point**:

| Point | Condition | Default threshold |
|---|---|---|
| 1 | GSR level elevated | `gsrLevel ≥ 0.55` |
| 2 | GSR rising fast | `gsrSlope ≥ 0.08` per second |
| 3 | HRV low | `beatValid` **and** `rmssdMs < 30` |
| 4 | Sound sustained | `sound ≥ 0.50` for `≥ 1.0 s` |

### 6.3 The motion gate
`moving = motionMagnitude ≥ 0.12` (deviation from 1 g, in g).

### 6.4 State decision

| Result | Rule |
|---|---|
| **DISTRESS** | `points ≥ 3` **AND** not moving |
| **ANXIOUS** | `points ≥ 1` (allowed while moving; never escalates past ANXIOUS) |
| **CALM** | otherwise (0 points) |

Rationale: a GSR/HR spike while moving is far more likely exertion than pain, so it can
never reach DISTRESS.

### 6.5 Output activation
- **RGB LED**: colour follows the state immediately — blue (CALM), orange (ANXIOUS),
  red (DISTRESS). Updated every 500 ms.
- **Buzzer** (and **motor**, full sketch): fire only after **DISTRESS has held for
  `ALERT_DELAY_MS` = 2.5 s**. Leaving DISTRESS resets the timer and silences them. This stops
  a single noisy sample from triggering an alarm.
- **Button** (full sketch): each press logs a timestamped **caregiver event mark**; it does
  not change the state.

### 6.6 What the minimal build can reach
`empathy_band_min` stubs sound = 0 and motion = 0 (still), so:
- only 3 conditions are live: GSR level, GSR slope, HRV low → **DISTRESS is reachable at
  exactly 3 points**;
- because motion reads 0 (still), the motion gate never blocks DISTRESS.
- The buzzer still waits the full 2.5 s of sustained DISTRESS.

> Note in your report: with the minimal build the motion gate and sound input are stubbed,
> so the DISTRESS path is easier to trigger than in the full system.

---

## 7. Serial telemetry (what the dashboard reads)

Once calibration is done the firmware prints one line every 100 ms:

```
[band] state=CALM gsr=0.31 slope=0.02 sound=0.00(0.0s) mot=0.00 bpm=74 rmssd=41
```

and, in the full sketch, on a button press:

```
[button] caregiver event mark @ 12345678 us
```

The dashboard (`dashboard/index.html`) parses exactly these two formats, so it works with
either sketch over **Web Serial** or as a standalone demo.

---

## 8. Quick reference

| Want to change… | Edit in `config.h` |
|---|---|
| Which pin a part uses | `PIN_*` |
| How easily DISTRESS triggers | `DISTRESS_MIN_POINTS`, `GSR_*_THRESH`, `RMSSD_LOW_THRESH_MS` |
| How long before the buzzer | `ALERT_DELAY_MS` |
| Buzzer screams at boot | `BUZZER_ACTIVE_LOW 1` |
| How long GSR calibrates | `GSR_CAL_MS` |
| Loop timing | `SENSOR_PERIOD_MS`, `ACTUATOR_PERIOD_MS` |

State colours and PWM setup live in `actuators.cpp`; the decision rules live in
`classify.cpp`.


## 8. Frontend Options

### 8.1 Web Dashboard (dashboard/index.html)
Original HTML/JS dashboard with Web Serial support for live monitoring.

### 8.2 Python Streamlit Dashboard (python_dashboard/)
New Python-based dashboard with enhanced visualizations:
- Real-time plotting using Plotly
- Serial communication via pyserial
- Demo mode for testing
- Session statistics and event logging
- Run with: `streamlit run python_dashboard/app.py`

### 8.3 Wokwi Simulation (wokwi/)
Online simulation environment:
- Visit [wokwi.com](https://wokwi.com) and import the project
- Use `diagram.json` for circuit layout
- Simulate sensors with potentiometers
- Monitor serial output in browser
