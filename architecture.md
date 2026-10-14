# Empathy Band — Architecture

**Author:** Pratyush Nandi (24BIT0282)
**Platform:** ESP32 DevKit (USB-C), ESP-IDF firmware
**Status:** Prototype on breadboard, 2-week build

---

## 1. Problem Statement

In late-stage Alzheimer's, dementia and other conditions that take away speech, patients cannot tell a caregiver that they are in pain or distressed. Unmet pain and anxiety then show up as agitation or aggression, which is often misread as a behavioural problem instead of a physical need. Caregivers have no objective, continuous signal to go on.

**Goal:** a wrist-worn device that continuously measures physiological and behavioural indicators of stress/pain, rejects readings that are explained by ordinary movement, classifies the patient as **Calm / Anxious / Distress**, and alerts the caregiver locally (light, vibration, sound).

**Why an ESP32 and not a Raspberry Pi:** the device is worn, so it needs to be small and low-power. The ESP32 has native ADC inputs (a Pi has none), hardware I2C, and RMT for LED timing. No OS is needed for this workload.

**Non-goals (be upfront about these in the report):** this is not a medical device. Heart rate and HRV are trend estimates from a simplified beat detector, not clinical measurements. SpO2 is not computed. Thresholds are untuned heuristics until you log real sessions.

---

## 2. Hardware Inventory (what you actually have)

| # | Part | Role | Interface |
|---|------|------|-----------|
| 1 | GSR module (Grove-style, analog) | Skin conductance (sweat response) | Analog |
| 2 | LM393 sound module (mic + comparator) | Vocalisation / ambient noise level | Analog (AOUT) |
| 3 | MPU6050 (GY-521 board) | Movement; used to reject false positives | I2C 0x68 |
| 4 | MAX30102 module | PPG heart rate / beat intervals | I2C 0x57 |
| 5 | Tactile push button | Caregiver event mark (control input, not a transducer) | Digital in |
| A1 | Active buzzer module | Audible alert | Digital out |
| A2 | Coin vibration motor | Haptic alert | Digital out via transistor |
| A3 | RGB LED module | Status colour | See note in section 5 |
| — | Full-size breadboard, M-M and F-F jumpers | Wiring | — |

**Not used:** DS18B20 (spare), MAX30205 (not purchased). Skin temperature is dropped from the design.

---

## 3. System Architecture

```
 SENSE                          PROCESS (ESP32)                      ACT
 ─────                          ───────────────                      ───
 GSR module ──analog──► GPIO34 ─┐
 LM393 sound ─analog──► GPIO35 ─┤
                                │   sensor_task (100 ms)
 MPU6050 ──┐                    ├──► gsr level + slope
 MAX30102 ─┴─ I2C ─► GPIO21/22 ─┤    sound level
                                │    motion magnitude (IMU)
 Button ──────────────► GPIO4 ──┘    beat detect → BPM → HRV
                                          │
                                          ▼
                                     classify()          pure function,
                                   CALM / ANXIOUS /      no I/O inside
                                      DISTRESS
                                          │  g_state
                                          ▼
                                    actuator_task (500 ms)
                                          │
                       ┌──────────────────┼───────────────────┐
                       ▼                  ▼                   ▼
                 RGB LED (GPIO25)   Buzzer (GPIO33)   Vibration motor
                 blue/orange/red    after ~2.5 s      (GPIO32 → transistor)
                                    sustained DISTRESS
```

### Data flow

1. **sensor_task** runs every 100 ms. It reads GSR and sound from ADC1, reads the IMU, drains the MAX30102 FIFO into the beat detector, derives BPM and a crude RMSSD, and packs everything into a `band_inputs_t`.
2. **classify()** scores the inputs and returns a state. It is a pure function, so it can be tested on a PC with no hardware.
3. **actuator_task** runs every 500 ms and maps state to outputs. Buzzer and motor only fire after DISTRESS has held ~2.5 s, so a single noisy sample can't trigger them.
4. **button ISR** pushes a timestamp to a queue; a task debounces and logs it. No work is done inside the ISR.

### Classification logic

Each satisfied condition adds one point:

| Signal | Condition | Meaning |
|---|---|---|
| GSR level | above normalised threshold | elevated sweat response |
| GSR slope | rising faster than threshold | sudden arousal |
| HRV (RMSSD) | below threshold | sympathetic activation |
| Sound level | above threshold, sustained | moaning / crying / agitation |

- **3+ points and IMU still → DISTRESS**
- **1+ points → ANXIOUS** (allowed while moving, never escalated past ANXIOUS)
- **else → CALM**

Motion gating is the key design decision: a GSR/HR spike during movement is far more likely exertion than pain, so it never reaches DISTRESS.

---

## 4. Pin Map (single source of truth)

| Function | ESP32 GPIO | Notes |
|---|---|---|
| I2C SDA | **21** | MPU6050 + MAX30102 share it |
| I2C SCL | **22** | MPU6050 + MAX30102 share it |
| GSR analog in | **34** | ADC1_CH6, input-only |
| Sound analog in | **35** | ADC1_CH7, input-only |
| Push button | **4** | other leg to GND, internal pull-up |
| RGB LED data | **25** | WS2812 case; see section 5 |
| Vibration motor | **32** | via NPN transistor, never direct |
| Buzzer | **33** | active module |
| 3V3 | 3V3 pin | sensor + LED supply rail |
| GND | any GND | common ground rail |

**Rules behind these choices**

- **GPIO34 and GPIO35 are different pins.** GSR and sound are both analog and cannot share an ADC pin. (An earlier suggestion to put the sound sensor on GPIO34 only worked as a *replacement* for GSR, not alongside it.)
- Analog inputs stay on **ADC1** (GPIO32–39). ADC2 pins stop working when Wi-Fi is active.
- GPIO34–39 are input-only, which is fine for sensors, and they have no internal pull-ups.
- Go by the **GPIO number**, not the silkscreen "D" label — these differ between ESP32 board variants.

---

## 5. What Connects to What

Power everything sensor-side from the **3V3** pin. Put 3V3 on one breadboard rail and GND on the other, and tap them per module.

### 5.1 MPU6050 (GY-521) — I2C

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail |
| GND | GND rail |
| SDA | GPIO21 |
| SCL | GPIO22 |
| AD0, INT, XDA, XCL | leave unconnected (AD0 floating/low = address 0x68) |

### 5.2 MAX30102 — I2C

| Module pin | Connect to |
|---|---|
| VIN | 3V3 rail |
| GND | GND rail |
| SDA | GPIO21 |
| SCL | GPIO22 |
| INT | leave unconnected |

Both I2C devices go on the **same two wires** (GPIO21/22). Different addresses (0x57, 0x68), so no conflict. Power the MAX30102 from 3.3 V, not 5 V.

### 5.3 GSR module — analog

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail (keeps output ≤ 3.3 V, safe for the ADC) |
| GND | GND rail |
| SIG / AOUT | GPIO34 |
| NC (Grove connector) | leave unconnected |

If it came with a Grove 4-pin cable, strip or use the cable's wires: black = GND, red = VCC, yellow = SIG, white = NC. Clip the two finger electrodes to two fingers of one hand.

### 5.4 LM393 sound module — analog

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail |
| GND | GND rail |
| AOUT | GPIO35 |
| DOUT | leave unconnected |

### 5.5 Push button

A tactile button has 4 legs; the two on opposite diagonals are the switch. Use diagonal legs:

- Leg A → GPIO4
- Leg B (diagonal) → GND

No external resistor is needed; the pull-up is enabled in firmware. The button reads LOW when pressed.

### 5.6 Buzzer (active module)

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail |
| GND | GND rail |
| I/O (signal) | GPIO33 |

**Check on first power-up:** many cheap buzzer modules are **active-low** (they sound when the signal is LOW). If it screams continuously at boot, invert the logic in `actuators.c`.

### 5.7 Vibration motor — needs a driver circuit

The GPIO cannot supply enough current for a motor. Build this on the breadboard (resistor, transistor and diode have bare legs, which is why a breadboard is required):

```
GPIO32 ──[ 1 kΩ ]──► Base ┐
                           │  2N2222 (NPN)
3V3 ──────┬── Motor (+)    │
          │                │
        [1N4001]      Collector ── Motor (−)
        (stripe        Emitter ──── GND
         toward 3V3)
        across motor
```

- **Diode** (1N4001) across the motor terminals, stripe toward the **+** side. It absorbs the voltage spike when the motor switches off and protects the transistor and ESP32.
- 2N2222 pinout depends on package (TO-92 is E-B-C in the common orientation, but check your part's markings).
- If your motor came **on a small module board with VCC/GND/IN pins**, it already has a driver: wire it like the buzzer (VCC→3V3, GND→GND, IN→GPIO32) and skip the transistor circuit.

### 5.8 RGB LED — unknown type, check the board in the hostel

**Case A: addressable (WS2812) module** — pins VCC, GND, DIN (maybe DOUT). This matches the current firmware.

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail (keeps data-line logic levels matched) |
| GND | GND rail |
| DIN | GPIO25 |
| DOUT | leave unconnected |

**Case B: plain RGB LED module** — 4 pins labelled R, G, B and `-` (common cathode) or `+` (common anode). The current WS2812 driver will **not** work with this; it needs a PWM driver instead.

| Module pin | Connect to |
|---|---|
| R | GPIO25 (through 220 Ω if the board has no resistors) |
| G | GPIO26 |
| B | GPIO27 |
| `-` (common cathode) | GND |
| `+` (common anode) | 3V3, and output logic is inverted |

### 5.9 Jumper and breadboard guidance

- Use the **full-size 830-point breadboard**. Seat the ESP32 across the centre gap.
- **M-M jumpers** for everything on the breadboard (ESP32 pin → hole, module pin → hole, rail taps).
- F-F jumpers are only for direct module-pin-to-ESP32-pin links that skip the breadboard. Keep them as spares.
- Expect roughly 20 wires total.

---

## 6. Build & Bring-up Order

Test one thing at a time. Do not wire everything and then debug.

1. ESP32 alone: flash a blink / serial print. Confirms toolchain and USB-C data cable (some USB-C cables are charge-only).
2. Add I2C devices **one by one** and run an I2C scan. Expect 0x68 (MPU6050) and 0x57 (MAX30102).
3. Add GSR and sound; print raw ADC values. Touch the electrodes, clap near the mic, confirm values move.
4. Add button, LED, buzzer; confirm each in isolation.
5. Add the motor circuit last.
6. Only then enable classification.

---

## 7. Risks and Open Items

| Item | Status | Action |
|---|---|---|
| Firmware still references **MAX30205** (skin temp) | Will fail at boot: `ESP_ERROR_CHECK` aborts when the device doesn't ACK | Remove `max30205` from `main.c` and `CMakeLists.txt` |
| Firmware has **no sound input** | `classify()` still expects skin temperature | Add `sound_adc` driver on GPIO35, replace temperature fields in `band_inputs_t` |
| RGB LED type unknown | Case A matches firmware; Case B needs a PWM driver | Check the board |
| Buzzer polarity unknown | Might be active-low | Test on first power-up |
| Vibration motor may be bare or on a module | Changes whether the transistor circuit is needed | Check the part |
| Thresholds are untuned | GSR baselines vary a lot person to person | Log real sessions, add a ~30 s rest calibration at boot |
| Beat detector is simplified | Not the Maxim reference algorithm | State this in the report |

---

## 8. References

- ESP-IDF Programming Guide (Espressif): ADC, I2C, RMT, GPIO. https://docs.espressif.com/projects/esp-idf/
- ESP32 Technical Reference Manual — ADC1/ADC2 channel mapping and the ADC2/Wi-Fi restriction.
- MAX30102 datasheet (Analog Devices): FIFO layout, register map.
- Maxim application note AN6409, *Recommended HRM/SpO2 Algorithm*: the reference beat-detection approach this project simplifies.
- MPU-6000/MPU-6050 Register Map and Descriptions, rev 4.2 (InvenSense/TDK).
- Seeed Studio Grove GSR sensor wiki: https://wiki.seeedstudio.com/Grove-GSR_Sensor/ (the module is an LM324-based analog front end).
- WS2812B datasheet: bit timing and supply-voltage logic thresholds.
- Task Force of the ESC and NASPE (1996), *Heart rate variability: standards of measurement*, Circulation 93(5): definition of RMSSD.
