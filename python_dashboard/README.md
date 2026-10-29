# Empathy Band - Python Dashboard

A Python-based dashboard for monitoring the Empathy Band IoT project using Streamlit.

## Features
- Real-time visualization of sensor data
- Classification breakdown matching firmware logic
- Event logging (state changes, alerts, caregiver marks)
- Session summary and statistics
- Demo mode with simulated data
- Serial port connection support

## Installation
```bash
pip install streamlit plotly pandas numpy pyserial
```

## Usage
```bash
streamlit run app.py
```

The dashboard will open in your browser at http://localhost:8501

## Modes
- **Demo Mode**: Generate simulated data for testing without hardware
- **Serial Port**: Connect directly to ESP32 via serial port

## Compatibility
This dashboard mirrors the logic from the HTML dashboard and firmware:
- Same classification rules (GSR, HRV, Sound, Motion)
- Same thresholds from config.h
- Parses the same serial format: `[band] state=... gsr=... slope=... sound=...(Xs) mot=... bpm=... rmssd=...`
- Handles caregiver event marks: `[button] caregiver event mark @ ... us`
