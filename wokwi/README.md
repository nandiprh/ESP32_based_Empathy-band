# Wokwi Simulation for Empathy Band

This directory contains Wokwi configuration for simulating the Empathy Band project.

## Files
- `diagram.json` - Circuit diagram for Wokwi (ESP32 with RGB LEDs, buzzer, button, and potentiometers for GSR/Sound simulation)
- `wokwi.toml` - Wokwi project configuration

## Usage
1. Go to [wokwi.com](https://wokwi.com)
2. Create a new ESP32 project
3. Copy the contents of `empathy_band/` folder into the project
4. Replace `diagram.json` and add `wokwi.toml` as needed
5. Use potentiometers to simulate GSR and Sound sensor values
6. Monitor serial output at 115200 baud

## Notes
- I2C sensors (MPU6050, MAX30102) may have limited support in Wokwi - for full simulation, some libraries may need adjustment
- The potentiometers simulate analog sensor readings for GSR (GPIO34) and Sound (GPIO35)
- RGB LEDs show the classification state (Blue=CALM, Orange/Yellow=ANXIOUS, Red=DISTRESS)
- Buzzer activates on sustained DISTRESS after alert delay
