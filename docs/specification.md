# Backend & Frontend Specification

This document covers both the firmware/backend (ESP32 Arduino sketches) and the caregiver frontend (dashboard). For hardware, see `connection_diagram.md` and `circuit_diagram.md`.

## 1. Firmware/backend specification

### 1.1 Platform & build
- Target: ESP32 DevKit (WROOM/compatible)
- Framework: Arduino-ESP32 core
- Sketches:
  - `empathy_band/` — full (GSR, Sound, MPU6050, MAX30102, RGB, Buzzer, Motor, Button)
  - `empathy_band_min/` — minimal (GSR, MAX30102, RGB, Buzzer)
- No external libraries required (uses Wire, ADC, LEDC PWM). Arduino IDE settings in `docs/codebase_guide.md`.

### 1.2 Runtime configuration
Source: `config.h` (both sketches; minimal omits unused pins/defines conceptually). Key values:
- Timing: `SENSOR_PERIOD_MS=100`, `ACTUATOR_PERIOD_MS=500`
- GSR calibration: `GSR_CAL_MS=30000`, `GSR_SPAN_COUNTS=1500.0`, polarity `GSR_STRESS_RAISES_OUTPUT`
- Thresholds: `GSR_LEVEL_THRESH=0.55f`, `GSR_SLOPE_THRESH=0.08f`, `RMSSD_LOW_THRESH_MS=30.0f`, `MOTION_STILL_THRESH=0.12f`, `SOUND_THRESH=0.5f`, `SOUND_SUSTAIN_MS=1000`, `DISTRESS_MIN_POINTS=3`
- Alert: `ALERT_DELAY_MS=2500` (sustained DISTRESS before buzzer/motor)
- Polarity: `BUZZER_ACTIVE_LOW` (0 default)

### 1.3 Data model
`BandInputs` (classify input):
- `float gsrLevel` [0..1] (normalized GSR)
- `float gsrSlope` [per second] (derivative of gsrLevel)
- `float soundLevel` [0..1]
- `bool  soundSustained`
- `float motionMagnitude` [g] (||a||-1g approx)
- `float bpm` [beats/min]
- `float rmssdMs` [ms]
- `bool  beatValid`

`BandState`: `STATE_CALM=0`, `STATE_ANXIOUS=1`, `STATE_DISTRESS=2`.

### 1.4 Feature extraction
- GSR: raw ADC (12-bit) averaged over 30 s rest → baseline. Normalized to [0,1] using span; slope computed in main loop over dt (µs).
- Sound (full): burst peak-to-peak amplitude scaled [floor..ceil] → [0,1]. `soundSustained` = norm >= SOUND_THRESH for >= SOUND_SUSTAIN_MS (tracked in loop).
- Motion (full): accelerometer magnitude deviation from ~1 g; `moving` if >= 0.12 g. Minimal: fixed 0.
- HR/HRV (MAX30102): IR FIFO drained, DC estimated by EMA, rising zero-cross detection, 300 ms refractory, rolling last 8 R-R intervals. BPM = 60000/mean R-R; RMSSD = sqrt(mean((ΔRR)^2)) over valid intervals; `beatValid` once enough beats collected.

### 1.5 Classifier rules (deterministic)
Points:
1. `gsrLevel >= GSR_LEVEL_THRESH`
2. `gsrSlope  >= GSR_SLOPE_THRESH`
3. `beatValid && rmssdMs < RMSSD_LOW_THRESH_MS`
4. `soundSustained`

Motion gate: `moving = motionMagnitude >= MOTION_STILL_THRESH`
State:
- DISTRESS if (`points >= DISTRESS_MIN_POINTS`) AND (`!moving`)
- else ANXIOUS if (`points >= 1`)
- else CALM

Effects: RGB color by state (CALM/ANXIOUS/DISTRESS). Buzzer/motor (full) only when DISTRESS has persisted >= `ALERT_DELAY_MS` (timer resets on leaving DISTRESS).

### 1.6 I/O interfaces
- Serial UART: 115200 bps, newline-delimited text logs
- I2C: Wire @ 400 kHz (GPIO21 SDA, GPIO22 SCL)
- ADC: GPIO34, GPIO35 (ADC1), 12-bit, `analogReadResolution(12)`
- PWM (RGB): LEDC 5000 Hz, 8-bit, channels 0(R),1(G),2(B), supports core 2.x/3.x
- GPIO: button INPUT_PULLUP (GPIO4, FALLING interrupt), buzzer/motor outputs

### 1.7 Telemetry format (backend → frontend)
Every 100 ms (after calibration):
```
[band] state=%s gsr=%.2f slope=%.2f sound=%.2f(%.1fs) mot=%.2f bpm=%.0f rmssd=%.0f
```
Where `state` is "CALM"/"ANXIOUS"/"DISTRESS".

On button press (full sketch):
```
[button] caregiver event mark @ %lu us
```
Parser in `dashboard/index.html` expects exactly these forms.

### 1.8 Non-blocking loop
- Sensor step every 100 ms
- Actuator update every 500 ms
- Button ISR flags press with micros() timestamp; main loop debounces
- No `delay()` in loop body

### 1.9 Error/robustness
- GSR requires 30 s rest calibration; logs progress percentage until calibrated
- MAX30102 init clears FIFO; beat detection enforces refractory
- IMU presence checked (optional) in full build
- Polarity guard via `BUZZER_ACTIVE_LOW`

### 1.10 Activation conditions (summary)
System becomes "active/monitoring" after boot: calibration runs first (state CALM, outputs reflect CALM). After calibration completes, classification runs continuously. DISTRESS triggers alerts only after sustained DISTRESS >= 2.5 s. Motion gate suppresses DISTRESS during movement.

---

## 2. Frontend specification (dashboard/index.html)

### 2.1 Purpose
Standalone caregiver dashboard to visualize live telemetry from the ESP32 over Web Serial, or run in Demo mode without hardware. Self-contained (no build tools, no external CDNs).

### 2.2 Structure
Single HTML file with embedded CSS and JavaScript:
- Header (title, connection/status, mode selector)
- Status cards: State, BPM, RMSSD (HRV), GSR, Slope, Sound, Motion
- Visuals: LED indicator, timeline canvas (state over time)
- Controls: Demo play/pause/reset, clear log, export CSV
- Details: Classifier breakdown (points + gate), Event log, Session summary

### 2.3 Modes
- **Demo**: synthetic telemetry generated with gentle variation to exercise UI/state machine. Useful for demos/screenshots.
- **Live (Web Serial)**: connects to ESP32 via `navigator.serial`. Requires Chrome/Edge, HTTPS or localhost, and user gesture to request port.

### 2.4 UI/UX
- Responsive, high-contrast, monospace for values/logs
- State colours: CALM (#4a90e2), ANXIOUS (#f5a623), DISTRESS (#d0021b)
- LED dot reflects current state
- Timeline canvas (600x180) plots state over last ~60 s (time axis moves left)
- Real-time updates at incoming line rate (~10 Hz)

### 2.5 Data parsing
Regex-based:
- Band line: `/^\[band\]\s+state=(\w+)\s+gsr=([0-9.]+)\s+slope=([0-9.]+)\s+sound=([0-9.]+)\(([0-9.]+)s\)\s+mot=([0-9.]+)\s+bpm=([0-9.]+)\s+rmssd=([0-9.]+)$/`
- Button line: `/^\[button\]\s+caregiver event mark @\s+(\d+)\s+us$/`

Parsed values update state, cards, timeline, breakdown, log.

### 2.6 Classifier breakdown (computed client-side)
Mirrors firmware logic using thresholds (defaults match `config.h`):
- Points: gsrLevel>=0.55, gsrSlope>=0.08, beatValid && rmssd<30, soundSustained
- moving = motionMagnitude>=0.12
- Result = CALM/ANXIOUS/DISTRESS per same rules
Displayed as checkboxes/values to explain "why" the state is what it is.

### 2.7 Session tracking & export
- Session start time, duration, counts: total samples, state transitions, events (button marks), time spent in each state
- CSV export: `timestamp,ms,state,gsr,slope,sound,soundS,mot,bpm,rmssd,event` (one row per sample; event flag on button lines)
- Event log shows button marks with absolute/relative time

### 2.8 Browser compatibility
Requires `navigator.serial` (Web Serial API). Supported in Chromium-based browsers (Chrome 89+, Edge). If API unavailable, UI shows notice; Demo still works.

### 2.9 Connection flow (Live)
1. Click "Connect" → `navigator.serial.requestPort()` (filters USB/serial)
2. Open at 115200 baud, 8N1
3. Read loop via `TextDecoderStream` or `port.readable.getReader()` + line splitting (`\n`, `\r`)
4. On disconnect/close, UI reverts to disconnected state

### 2.10 Demo generation
Synthetic waveform: state evolves slowly (CALM→ANXIOUS→DISTRESS→CALM) with small noise on gsr/slope/sound/motion/bpm/rmssd; emits button events occasionally; timestamps advance realistically.

### 2.11 Styling
Embedded CSS uses variables, grid layout, soft shadows, canvas styling. Monospace font for numeric/log output. Responsive down to smaller viewports.

### 2.12 No backend server required
Pure static HTML/JS; runs from `file://` for demo, or served over HTTP(S) for Web Serial (works well from local filesystem in modern browsers with user gesture, and best over localhost).
