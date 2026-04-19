# Airport Flow
### A Kinetic Data Sculpture

![Preview](media/KINETIC_still.jpg)

*An exploration of translating invisible systems into physical motion.*

Four rods move continuously — reflecting the rhythm of arrivals and departures across New York City's airports. What is normally abstract data becomes something you can watch.

---

## 🎥 Demo

![Demo](media/KineticDemoGif.gif)

[▶ Watch Full Demos](media/)

---

## Concept

Air travel is constant movement — arrivals, departures, rhythm.

Four rods move continuously — each reflecting a live stream of arrivals or departures across New York City's airports.

The system reduces a complex, distributed network into a simple physical language:

```
Left  → arrivals
Right → departures
Angle → intensity
```

Angle encodes traffic level differently for each side:

```
Left rods  (arrivals):   90° = high  |  180° = low
Right rods (departures): 90° = high  |    0° = low
```

At any moment, the sculpture reflects the live state of air travel through the city.

---

## How It Works

### Data Pipeline

The system polls the **AeroDataBox API** (via RapidAPI) at a fixed interval (~60 seconds), pulling live arrival and departure counts for JFK and LGA — four independent values mapped to physical output.

Raw counts are **normalized** against observed min/max traffic ranges and mapped to servo angles between 90° and 180°. This keeps physical motion stable and legible regardless of time of day or traffic volume.

**Error handling:** Failed API requests are caught and the last known good values are held, preventing erratic motion during network interruptions.

### Hardware Control

The ESP32 communicates with a **PCA9685 PWM servo driver** over I2C, controlling four SG90 servos independently with precise PWM signaling. Each servo drives a carbon fiber rod via pushrod linkages.

**Smoothing logic** interpolates between current and target angles over multiple cycles, eliminating abrupt mechanical jumps and producing fluid, continuous motion that reads as organic rather than mechanical.

The ESP32 manages the full control loop — from data ingestion through normalization and actuation — ensuring consistent, stable physical output in real time.

### Architecture

```text
AeroDataBox API
      ↓
  HTTP polling (ESP32)
      ↓
  Normalization + mapping
      ↓
  Smoothing interpolation
      ↓
  PCA9685 (I2C)
      ↓
  SG90 Servos → Carbon fiber rods
```

---

## Components

| Component | Role |
|---|---|
| ESP32 | Main controller, WiFi, API polling |
| PCA9685 | 16-channel PWM servo driver (I2C) |
| SG90 Servos (×4) | Motion actuators |
| Carbon fiber rods | Visual output elements |
| Pushrod linkages | Mechanical translation |
| 5V regulated power | Servo power supply |

---

## Build & Setup

1. Clone the repo
2. Open `arduino/` in Arduino IDE
3. Install dependencies:
   - `Adafruit PWM Servo Driver Library`
   - `ArduinoJson`
   - `HTTPClient` (included with ESP32 board package)
4. Create a `secrets.h` file in `arduino/`:
```cpp
#define WIFI_SSID     "your_network"
#define WIFI_PASSWORD "your_password"
#define RAPIDAPI_KEY  "your_key"
```
5. Flash to ESP32

---

## Design Decisions

**Tilt over vertical movement** — servo tilt reduces mechanical complexity while increasing visual legibility at a distance.

**No screen, no dashboard** — the constraint of physical output forced cleaner data reduction. One angle per airport. Nothing else.

**Smoothing over precision** — the goal is presence, not accuracy. Interpolated motion reads as alive. Snapped motion reads as broken.

---

## Constraints

- API rate limits required careful polling frequency management to stay within free tier limits
- Mechanical linkage introduced precision and stability tradeoffs — smooth motion required tuning interpolation to compensate for servo and linkage play

---

## Next

- Improve mechanical precision and reduce linkage play
- Expand to additional airports and higher-frequency updates
- Integrate additional data sources (weather, wind, delays)

---

## Context

Built in one week as a focused exploration of physical computing, real-time systems, and motion as a data medium.

---

## Author

Michael Johnston
