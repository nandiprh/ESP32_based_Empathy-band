# Empathy Band — Connection Diagram & Wiring Guide

This document is the wiring reference for the Empathy Band prototype. It matches the pin
assignments compiled into the Arduino sketch (`empathy_band/config.h`). Use the **GPIO
number**, not the silkscreen "D" label — those differ between ESP32 board variants.

Device: ESP32 DevKit (USB-C). Firmware: Arduino IDE sketch in `empathy_band/`.

---

## 1. Pin map (single source of truth)

| Function | ESP32 GPIO | Direction | Notes |
|---|---|---|---|
| I2C SDA | **21** | I/O | MPU6050 + MAX30102 share this line |
| I2C SCL | **22** | I/O | MPU6050 + MAX30102 share this line |
| GSR analog in | **34** | input | ADC1_CH6, input-only, no internal pull-up |
| Sound analog in | **35** | input | ADC1_CH7, input-only, no internal pull-up |
| Push button | **4** | input | other leg to GND, internal pull-up enabled |
| RGB LED red | **25** | output | PWM channel, common-cathode module (HW-478) |
| RGB LED green | **26** | output | PWM channel |
| RGB LED blue | **27** | output | PWM channel |
| Vibration motor | **32** | output | never drive the motor directly; use transistor |
| Buzzer | **33** | output | active buzzer module |
| 3V3 | 3V3 pin | power | sensor + LED supply rail |
| GND | any GND | power | common ground rail |

Rules behind these choices:

- **GPIO34 and GPIO35 are different pins.** GSR and sound both need an ADC channel and
  cannot share one pin.
- Analog inputs stay on **ADC1** (GPIO32–39). ADC2 pins stop working while Wi-Fi is active.
- GPIO34–39 are **input-only** — fine for sensors, but they have no internal pull-ups.
- GPIO21/22 are the default I2C pins for the ESP32 Arduino core, so `Wire.begin(21, 22)`
  works with no remapping.

---

## 2. Power rails first (do this before any signal wire)

Seat the ESP32 across the centre gap of the **full-size 830-point breadboard**. Then:

1. Run a **3V3** wire from the ESP32 3V3 pin to the **top positive rail**.
2. Run a **GND** wire from an ESP32 GND pin to the **top negative rail**.
3. Power every module from these two rails (VCC → 3V3 rail, GND → GND rail).

> Keep the ESP32 powered over USB-C during bring-up. Do **not** feed 5 V into the 3V3
> rail, and do not power the MAX30102 from 5 V.

```
ESP32 3V3 ────────────────┬── (+) rail ──── VCC of every module
                          │
ESP32 GND ────────────────┴── (−) rail ──── GND of every module
```

---

## 3. I2C bus (MPU6050 + MAX30102 on the same two wires)

Both I2C devices share **GPIO21 (SDA)** and **GPIO22 (SCL)**. They have different
addresses (`0x68` and `0x57`), so there is no conflict.

```
ESP32 GPIO21 (SDA) ──┬── MPU6050 SDA
                     └── MAX30102 SDA

ESP32 GPIO22 (SCL) ──┬── MPU6050 SCL
                     └── MAX30102 SCL

3V3 rail ────────────┬── MPU6050 VCC
                     └── MAX30102 VIN   (3.3 V, NOT 5 V)

GND rail ────────────┬── MPU6050 GND
                     └── MAX30102 GND
```

### 3.1 MPU6050 (GY-521)

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail |
| GND | GND rail |
| SDA | GPIO21 |
| SCL | GPIO22 |
| AD0 | leave unconnected (floating/low = address 0x68) |
| INT, XDA, XCL | leave unconnected |

### 3.2 MAX30102

| Module pin | Connect to |
|---|---|
| VIN | 3V3 rail |
| GND | GND rail |
| SDA | GPIO21 |
| SCL | GPIO22 |
| INT | leave unconnected |

> Confirm with an I2C scan after wiring: you should see **0x68** and **0x57**. If a device
> is missing, it is almost always SDA/SCL swapped or a module powered from the wrong rail.

---

## 4. GSR module (analog)

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail (keeps output ≤ 3.3 V, safe for the ADC) |
| GND | GND rail |
| SIG / AOUT | **GPIO34** |
| NC | leave unconnected |

If it came with a Grove 4-pin cable, the wire colours are:

| Wire | Signal |
|---|---|
| Black | GND |
| Red | VCC (3V3) |
| Yellow | SIG → GPIO34 |
| White | NC |

Clip the two finger electrodes to two fingers of the **same hand**. Keep the hand still
during the 30 s rest calibration that runs at boot.

---

## 5. LM393 sound module (analog)

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail |
| GND | GND rail |
| AOUT | **GPIO35** |
| DOUT | leave unconnected |

We use the analog output (`AOUT`); the comparator digital output (`DOUT`) is not used.

---

## 6. Push button

A tactile button has 4 legs; the two on opposite diagonals are the switch. Use diagonal
legs:

- Leg A → **GPIO4**
- Leg B (diagonal) → **GND**

No external resistor is needed — the sketch enables the internal pull-up. The button reads
**LOW when pressed**, and a press logs a caregiver event mark.

---

## 7. Buzzer (active module)

| Module pin | Connect to |
|---|---|
| VCC | 3V3 rail |
| GND | GND rail |
| I/O (signal) | **GPIO33** |

> **Check on first power-up:** many cheap buzzer modules are **active-low** (they sound when
> the signal is LOW). If the buzzer screams continuously at boot, set
> `#define BUZZER_ACTIVE_LOW 1` in `empathy_band/config.h` and re-upload.

---

## 8. Vibration motor (needs a driver circuit)

A GPIO cannot supply enough current for a motor. Build this on the breadboard:

```
                3V3
                 │
                 ├──────────────► Motor (+)
                 │
              [1N4001]           (diode across motor,
              stripe toward 3V3   stripe toward +)
                 │
                 ├──────────────► Motor (−)
                 │
GPIO32 ─[1kΩ]─► Base ┐
                     │  2N2222 (NPN)
   Collector ────────┘
   Emitter ────────────────► GND
```

- **Diode (1N4001)** goes across the motor terminals, stripe toward the **+** side. It
  absorbs the voltage spike when the motor switches off and protects the transistor and
  ESP32.
- **2N2222 pinout** depends on the package (TO-92 is commonly E-B-C, but check your part's
  markings).
- **If your motor came on a small module board** with VCC/GND/IN pins, it already has a
  driver: wire it like the buzzer (VCC → 3V3, GND → GND, IN → GPIO32) and skip the
  transistor circuit.

Maps to PM55 / NPN summary:

| Node | Connects to |
|---|---|
| GPIO32 → 1 kΩ → transistor Base | drive signal |
| 3V3 → motor (+) | supply |
| Motor (−) → Collector | switched return |
| Emitter → GND | ground |
| 1N4001 across motor | spike protection (stripe to +) |

---

## 9. RGB LED (confirmed: common-cathode module, HW-478)

Your module has 4 pins labelled **B, R, G, `-`**. The `-` pin is the **common cathode**
(shared negative). It is a plain 3-channel RGB LED module, **not** addressable — so the
firmware drives it with three PWM channels (one per colour), not a WS2812 data line.

| Module pin | Connect to |
|---|---|
| R | **GPIO25** |
| G | **GPIO26** |
| B | **GPIO27** |
| `-` (common cathode) | GND rail |

Notes:

- Common cathode means a **HIGH / PWM duty lights the LED**. The firmware is written for
  this; if you ever swap to a common-anode module, the logic inverts.
- The pin **order on the board** (B, R, G, `-`) does not matter — just match each labelled
  colour pin to its GPIO.
- The HW-478 board normally has on-board current-limiting resistors. If yours does **not**,
  add a ~220 Ω resistor in series with each of R, G, B.
- PWM is set to 5 kHz / 8-bit in `actuators.cpp`; no external library is required.


---

## 10. Full wiring checklist

| # | From | To | Wire |
|---|---|---|---|
| 1 | ESP32 3V3 | breadboard (+) rail | M-M |
| 2 | ESP32 GND | breadboard (−) rail | M-M |
| 3 | (+) rail | GSR VCC | M-M |
| 4 | (−) rail | GSR GND | M-M |
| 5 | GSR SIG | GPIO34 | M-M |
| 6 | (+) rail | Sound VCC | M-M |
| 7 | (−) rail | Sound GND | M-M |
| 8 | Sound AOUT | GPIO35 | M-M |
| 9 | (+) rail | MPU6050 VCC | M-M |
| 10 | (−) rail | MPU6050 GND | M-M |
| 11 | MPU6050 SDA | GPIO21 | M-M |
| 12 | MPU6050 SCL | GPIO22 | M-M |
| 13 | (+) rail | MAX30102 VIN | M-M |
| 14 | (−) rail | MAX30102 GND | M-M |
| 15 | MAX30102 SDA | GPIO21 | M-M |
| 16 | MAX30102 SCL | GPIO22 | M-M |
| 17 | Button leg A | GPIO4 | M-M |
| 18 | Button leg B (diagonal) | (−) rail | M-M |
| 19 | (+) rail | Buzzer VCC | M-M |
| 20 | (−) rail | Buzzer GND | M-M |
| 21 | Buzzer I/O | GPIO33 | M-M |
| 22 | GPIO32 | 1 kΩ → 2N2222 base | M-M |
| 23 | 3V3 | Motor (+) and 1N4001 cathode (stripe) | M-M |
| 24 | Motor (−) | 2N2222 collector | M-M |
| 25 | 2N2222 emitter | (−) rail | M-M |
| 26 | RGB module R | GPIO25 | M-M |
| 27 | RGB module G | GPIO26 | M-M |
| 28 | RGB module B | GPIO27 | M-M |
| 29 | RGB module `-` (cathode) | (−) rail | M-M |

Expect roughly 29 wires. F-F jumpers are only for direct module-pin-to-ESP32-pin links
that skip the breadboard; keep them as spares.

---

## 11. Bring-up order (test one thing at a time)

Do **not** wire everything and then debug.

1. **ESP32 alone**: flash a blink / serial print. Confirms the toolchain and that your
   USB-C cable carries data (some are charge-only).
2. **I2C, one device at a time**: run an I2C scan, expect `0x68` then `0x57`.
3. **GSR and sound**: print raw ADC values; touch the electrodes and clap near the mic to
   confirm the values move.
4. **Button, LED, buzzer**: confirm each in isolation.
5. **Motor circuit last**.
6. **Only then** enable classification (the full sketch).

---

## 12. Known gotchas

| Symptom | Likely cause |
|---|---|
| Buzzer screams at boot | active-low module → set `BUZZER_ACTIVE_LOW 1` |
| I2C device missing from scan | SDA/SCL swapped, or wrong supply rail |
| ADC readings stuck near 0 or 4095 | sensor on wrong pin / wrong rail / no common ground |
| Motor resets the ESP32 | missing/backwards 1N4001, or motor wired straight to GPIO |
| GSR never leaves calibration | fewer than 300 samples collected — wait the full 30 s |
| RGB shows nothing / wrong colours | check R→25, G→26, B→27 and that `-` goes to GND (not 3V3) |

---

## 13. Running with fewer sensors (low on jumpers)

There is a **separate minimal sketch** at `empathy_band_min/` that uses only **GSR,
MAX30102 (heart rate), the buzzer and the RGB LED** — no sound, IMU, motor or button. Use it
while you are short on jumpers. The full sketch in `empathy_band/` is unchanged.

Minimal-build wiring (only these parts):

| Wire | From | To |
|---|---|---|
| 1 | ESP32 3V3 | `+` rail |
| 2 | ESP32 GND | `−` rail |
| 3–5 | GSR VCC / GND / SIG | `+` rail / `−` rail / GPIO34 |
| 6–9 | MAX30102 VIN / GND / SDA / SCL | `+` rail / `−` rail / GPIO21 / GPIO22 |
| 10–13 | RGB R / G / B / `−` | GPIO25 / GPIO26 / GPIO27 / `−` rail |
| 14–16 | Buzzer VCC / GND / I/O | `+` rail / `−` rail / GPIO33 |

What it can do: with sound and motion stubbed out, **DISTRESS is reachable** from GSR level +
GSR slope + low HRV (3 points), and the buzzer fires after 2.5 s. The dashboard reads the
same serial lines from either sketch.

Everything else in this document (full wiring) still applies when you build the complete
system.



## 14. Per-sensor ASCII block diagram

### 14.1 ESP32 DevKit breakout (reference)
```
                ┌─────────────────────────────────────────┐
                │ ESP32 DevKit (Arduino IDE)               │
                │ 3V3  | GND | GPIO21(SDA) | GPIO22(SCL)    │
                │ GPIO34| GPIO35| GPIO4     | GPIO25(R)     │
                │ GPIO26(G)| GPIO27(B)| GPIO32(MOT)| GPIO33(BUZ)│
                └─────────────────────────────────────────┘
                         ▲      ▲
                         │      └───────────── rails 3V3 / GND
                         │
                      bus taps
```

### 14.2 MPU6050 (GY-521)
```
┌─────────────┐
│ MPU6050      │        ┌─────────────┐
│ VCC→3V3      │────────┤ + rail       │
│ GND→GND      │────────┤ − rail       │
│ SDA→GPIO21   │────────┤ ESP32        │
│ SCL→GPIO22   │────────┤ ESP32        │
└─────────────┘
```

### 14.3 MAX30102
```
┌─────────────┐
│ MAX30102     │        ┌─────────────┐
│ VIN→3V3      │────────┤ + rail       │
│ GND→GND      │────────┤ − rail       │
│ SDA→GPIO21   │────────┤ ESP32        │
│ SCL→GPIO22   │────────┤ ESP32        │
└─────────────┘
```

### 14.4 GSR module
```
┌─────────────┐
│ GSR          │        ┌─────────────┐
│ VCC→3V3      │────────┤ + rail       │
│ GND→GND      │────────┤ − rail       │
│ SIG→GPIO34   │────────┤ ESP32        │
└─────────────┘
  ├─ electrode A (finger)
  └─ electrode B (finger, same hand)
```

### 14.5 LM393 Sound module (analog only)
```
┌─────────────┐
│ LM393        │        ┌─────────────┐
│ VCC→3V3      │────────┤ + rail       │
│ GND→GND      │────────┤ − rail       │
│ AOUT→GPIO35  │────────┤ ESP32        │
│ DOUT         │        (leave open)
└─────────────┘
```

### 14.6 Push button
```
┌─────────────┐
│ Tactile      │        ┌─────────────┐
│ leg A→GPIO4  │────────┤ ESP32        │
│ leg B→GND    │────────┤ − rail       │
└─────────────┘
(diagonals of the 4-pin button)
```

### 14.7 Buzzer (active)
```
┌─────────────┐
│ Buzzer       │        ┌─────────────┐
│ VCC→3V3      │────────┤ + rail       │
│ GND→GND      │────────┤ − rail       │
│ I/O→GPIO33   │────────┤ ESP32        │
└─────────────┘
```

### 14.8 Vibration motor (driver: 2N2222 + 1N4001 + 1kΩ)
```
                    3V3 (+ rail)
                      │
                ┌─────┴─────┐
                │ 1N4001     │  (stripe toward 3V3 / +)
                │ flyback    │
                └─────┬─────┘
                      │
                   Motor (+)
                      │
                   Motor (−)
                      │
                ┌─────┴─────┐
                │ 2N2222 C  │
                └─────┬─────┘
                      │
                ┌─────┴─────┐
                │ 2N2222 E  │──────── GND (− rail)
                └───────────┘
                      │
                ┌─────┴─────┐
                │ 1kΩ       │
                └─────┬─────┘
                      │
                   GPIO32 (ESP32)
```

### 14.9 RGB LED (HW-478, common-cathode B,R,G,-)
```
┌─────────────┐
│ HW-478       │        ┌─────────────┐
│ R→GPIO25     │────────┤ ESP32        │
│ G→GPIO26     │────────┤ ESP32        │
│ B→GPIO27     │────────┤ ESP32        │
│ -→GND        │────────┤ − rail       │
└─────────────┘
```

### 14.10 Minimal build only (empathy_band_min)
Use only: GSR (14.4), MAX30102 (14.3), Buzzer (14.7), RGB (14.9). No sound, MPU6050, button, motor.
