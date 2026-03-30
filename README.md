# ForearmPad

> A wearable, touchless mouse input device for people with limited finger mobility — worn on the left forearm, controlled by the right hand.

ForearmPad turns a left forearm into a 15 cm virtual trackpad using an ultrasonic distance sensor and an ESP32 microcontroller. No finger dexterity required. No buttons to press. Gestures are detected by hovering and moving the right hand above the forearm, emitting Bluetooth HID mouse events to any connected device.

This project is being developed as a hardware input peripheral for the **[TimoS](https://github.com/theDapperFoxtrot/timos)** project by [the Dapper Foxtrot](https://github.com/theDapperFoxtrot) — an accessible input layer for VR that enables people who cannot use fine motor hand control to operate computers and VR applications autonomously.

---

## Why This Exists

Most computer input devices — mice, trackpads, keyboards — assume fully functional hand and finger dexterity. For people with conditions like ALS, cerebral palsy, spinal cord injury, or other motor impairments, standard input hardware is a barrier rather than a tool.

ForearmPad is designed from the ground up for **input without finger use**. The forearm is a large, stable body surface that remains accessible even when hand function is significantly reduced. By pointing a single ultrasonic sensor along the forearm axis, we get a continuous 1D position reading that maps cleanly onto left click, right click, and scroll — the three most essential mouse actions.

This is part of a broader effort in the TimoS ecosystem to provide **input redundancy**: every meaningful computer action should be triggerable through multiple modalities, so that a user only needs *one* working input method.

---

## How It Works

The HC-SR04 ultrasonic sensor is mounted at the **wrist end** of the left forearm, pointed toward the elbow. It continuously measures how far the right hand is from the sensor as it hovers above the arm.

```
[SENSOR] ←──────────── 14 cm ────────────→ [ELBOW]
          ← LEFT zone  │  RIGHT zone →
          (0 – 9 cm)   │  (9 – 14 cm)
             LEFT CLICK │ RIGHT CLICK
          ↑ scroll up  ←→  ↓ scroll down
```

### Gesture Logic

| Gesture | How to perform | Output |
|---|---|---|
| **Left click** | Briefly dip right hand above wrist-side half and lift | Left mouse button click |
| **Right click** | Briefly dip right hand above elbow-side half and lift | Right mouse button click |
| **Scroll up** | Sweep right hand toward the sensor (wrist direction) | Mouse scroll up |
| **Scroll down** | Sweep right hand away from sensor (elbow direction) | Mouse scroll down |
| **No action** | Hand held still | Nothing (hover, no event) |

Clicks require the hand to be present for **35–500 ms**. Any motion during that window escalates to scroll mode and suppresses the click, preventing accidental clicks while scrolling. A 400 ms cooldown after scroll sessions prevents re-entry ghost clicks.

### Signal Processing

Raw sensor readings are smoothed with an **Exponential Moving Average (EMA)** filter (α = 0.25) running at ~33 Hz to suppress the HC-SR04's inherent ±3 mm noise without introducing perceptible latency on gesture edges.

---

## Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32 DevKit V1 |
| Sensor | HC-SR04 Ultrasonic Distance Sensor |
| Connectivity | Bluetooth Low Energy (BLE HID) |

### Wiring

| HC-SR04 | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| Trig | GPIO5 (D5) |
| Echo | GPIO18 (D18) |

> **Note:** HC-SR04 is rated for 5V but operates at 3.3V with reduced max range (~100 cm). For a 14 cm pad this is more than sufficient. If your module produces unreliable readings at 3.3V, power VCC from the 5V (VIN) pin and add a voltage divider (1kΩ + 2kΩ) on the Echo line to protect the ESP32 GPIO.

CAD files for a 3D-printable forearm mount are included in the [`CAD Files/`](./CAD%20Files/) directory.

---

## Software Setup

### Dependencies

- **Arduino IDE** with the [ESP32 board package](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html) (espressif/arduino-esp32 ≥ 2.x)
- **[ESP32-BLE-Mouse](https://github.com/T-vK/ESP32-BLE-Mouse)** library by T-vK

### Known Issue: ESP32 Core 3.x Compatibility

If you are using ESP32 Arduino core **3.x**, the BLE-Mouse library has a type mismatch with the updated BLE API. Open `BleMouse.cpp` in your Arduino libraries folder and apply these two fixes:

**Line ~143:**
```cpp
// Before:
BLEDevice::init(bleMouseInstance->deviceName);
// After:
BLEDevice::init(String(bleMouseInstance->deviceName.c_str()));
```

**Line ~151:**
```cpp
// Before:
bleMouseInstance->hid->manufacturer()->setValue(bleMouseInstance->deviceManufacturer);
// After:
bleMouseInstance->hid->manufacturer()->setValue(String(bleMouseInstance->deviceManufacturer.c_str()));
```

### Flash

1. Open `ForearmPad.ino` in Arduino IDE
2. Select your ESP32 board and COM port
3. Upload

The device advertises itself over BLE as **"ForearmPad"**. Pair it like any Bluetooth mouse from your OS's Bluetooth settings.

---

## Tuning Parameters

All tuning constants are at the top of `ForearmPad.ino`:

| Parameter | Default | Effect |
|---|---|---|
| `PAD_MAX` | 14.0 cm | Total forearm pad length |
| `PAD_SPLIT` | 9.0 cm | Left / Right click zone boundary |
| `PAD_MIN` | 2.0 cm | Minimum distance (noise floor) |
| `EMA_ALPHA` | 0.25 | Smoothing factor — lower = smoother but more latency |
| `TAP_MIN_MS` | 35 ms | Minimum dwell to register a click |
| `TAP_MAX_MS` | 500 ms | Maximum dwell to still be a click (not a hold) |
| `SCROLL_DELTA` | 0.55 cm/sample | Minimum per-sample movement to enter scroll mode |
| `SCROLL_TICK` | 1.4 cm | Distance of hand travel per one scroll step |
| `POST_SCROLL_COOLDOWN_MS` | 400 ms | Lockout period after scroll ends, prevents ghost clicks |

---

## TimoS Integration

ForearmPad is being developed as a peripheral input device for **TimoS**, an open-source accessible input layer for VR built by [the Dapper Foxtrot](https://github.com/theDapperFoxtrot).

TimoS translates diverse inputs — head tracking, eye gaze, voice commands, controller collision, and assistive peripherals — into standard mouse, keyboard, and gamepad events. It runs as a SteamVR overlay, requiring no OS-level configuration, and is designed for people who cannot use conventional hand control to operate VR and desktop applications.

ForearmPad slots into the TimoS **input redundancy model**: users only need one working input method, and ForearmPad provides a hands-free, dexterity-free option for left/right click and scroll — the most critical mouse actions for navigating any interface.

> TimoS is currently in active development and not yet publicly available.

---

## Roadmap

- [ ] Multi-sensor version for 2D position tracking (absolute cursor movement)
- [ ] Double-tap gesture for drag lock
- [ ] Configurable zone split via serial command (no reflash needed)
- [ ] TimoS BLE client integration
- [ ] Battery-powered housing with forearm strap

---

## License

MIT

---

*Built as part of the accessibility hardware ecosystem for the TimoS project.*
