This was a project for my Embedded systems and IoT course, the ultimate aim is to design a wrist band, that actively monitors patients suffering late stage Alzheimer's, Dementia and other neurological diseases and abnormalities.
In late stage most of the time patients lose the ability to respond or signal comprehensively, which leads to frequent distress, that takes further physical and mental toll on patients.
This band sends alarm so the caregiver may act wisely on time, and reduce the distress of the suffering patient.

This project uses 5 sensors and 3 actuators, while the board used is ESP32.

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

System Architecture

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
