# Industrial Protection & Predictive Maintenance System

Dual-redundant embedded monitoring and protection platform for industrial three-phase machines, built on twin ESP32-S3 microcontrollers with real-time fault detection, fail-safe hardware watchdogs, and a cloud-connected predictive maintenance layer.

Developed as a final-year capstone (*mémoire de fin de formation*) for the Brevet de Technicien Supérieur (BTS) in Industrial Electronics at INSFP Kacem Cherif, Sétif, Algeria (April 2026), in the context of a technical internship at Sonelgaz's Aïn Arnat combined-cycle power plant (1,015 MW).

## Why this exists

Unplanned failures on industrial three-phase machinery are expensive and, in the energy sector particularly, can be dangerous. Commercial monitoring/protection systems that address this are typically costly and hard to retrofit onto older equipment. This project is a low-cost, retrofit-friendly alternative: a self-contained module that bolts onto existing machines, measures their vital signs across four physical domains, protects them in real time, and — over time — learns to anticipate failures before they happen.

## Key features

- **200% hardware redundancy** — two independent ESP32-S3 modules per machine, cross-validating every measurement over UART before any protective action is taken
- **Multi-domain sensing** — 7 sensors covering electrical, mechanical, environmental, and acoustic parameters
- **Sub-100 ms protection response**, enforced by a deterministic C/FreeRTOS firmware architecture
- **Two-tier fail-safe watchdog** — independent hardware watchdogs (ATtiny85 per module, NE555 at system level) guarantee the contactor opens even if both main MCUs fail completely
- **No silent auto-restart** — a tripped protection state requires explicit human confirmation to clear, by design
- **Secure inter-module pairing** — XTEA challenge-response authentication over ESP-NOW
- **Cloud-backed predictive maintenance** — trend analysis against a learned "health signature," distinct from instantaneous threshold protection
- **Cross-platform supervision app** — browser-based (HTML5/CSS3/JS), served directly from the gateway's own Wi-Fi access point, no app store dependency

## System architecture

Three functional tiers:

1. **Power & protection stage** — 380/400 V three-phase supply → differential breaker → Schneider TeSys D contactor (LC1D38BD, 24 V DC coil, 38 A AC-3). An NE555 hardware watchdog enforces fail-safe: any total failure of the electronics forces the contactor open.
2. **Protection modules (×2)** — Two ESP32-S3 WROOM-1 (N16R8) boards per machine:
   - **S3-A ("Informer")** — active measurement + local LCD/button/buzzer interface
   - **S3-B ("Silent")** — silent redundant measurement
   - Linked by UART at 115,200 baud for continuous cross-validation; each supervised by its own independent ATtiny85 hardware watchdog.
3. **Gateway / HMI bridge (×2)** — Two ESP32-WROOM-32 boards (primary + hot standby) receive data over ESP-NOW, aggregate it, and bridge it to the supervision app over Wi-Fi (AP+STA) and TCP.

**Data flow:** Acquisition → Local processing/filtering → Cross-validation (S3-A ↔ S3-B) → Protection decision → ESP-NOW transmission → Gateway aggregation → Supervision app (iPad/browser)

## Hardware

| Domain | Sensor / Component | Interface | Notes |
|---|---|---|---|
| Environmental | SHT45 (Sensirion) | I²C | Temp ±0.1 °C, humidity ±1.0 %RH |
| Electrical (current) | ACS758LCB-100B (Allegro, Hall-effect) | Analog | ±100 A, 4800 V AC galvanic isolation |
| Electrical (voltage) | ZMPT101B | Analog | 0–1000 V AC, 4000 V AC isolation |
| Vibration | LIS344ALH (STMicroelectronics MEMS) | Analog | 3-axis, auto-ranging ±2 g/±6 g, ISO 10816-aligned |
| Rotation | SS41F (Honeywell, bipolar Hall) | Digital | RPM via hardware pulse counter (PCNT) |
| Gas | MQ-2 / MQ-4 / MQ-9 | Analog (muxed via CD4053BE) | LPG/smoke, methane, and CO respectively |
| Acoustic | MAX9814 (Maxim, AGC mic amp) | Analog | Selectable 40/50/60 dB gain |
| Switching | Schneider TeSys D LC1D38BD | — | 38 A AC-3, 24 V DC coil, 1.4M electrical / 30M mechanical cycle life |
| Watchdog co-processor | ATtiny85 (×2) | — | Independent of main MCU; 0.5 µA power-down current |

Also present: SRD-05VDC / SRD-12VDC relay modules for low-voltage switching.

## Firmware

- **Language:** C (C11) — chosen over Arduino-style abstractions for direct register access, deterministic timing, and a hard <100 ms protection-response requirement
- **Framework:** ESP-IDF v5.5.3 (FreeRTOS, native driver stack, JTAG debugging, NVS for persistent config)
- **Core pinning (ESP32-S3):** Core 0 handles communication (Wi-Fi/ESP-NOW, UART peer link, watchdog heartbeat); Core 1 handles real-time acquisition, protection logic, and the local HMI — isolating network jitter from the protection loop
- **Persistent config (NVS):** alarm thresholds, paired MAC addresses, sensor calibration, role assignment — falls back to safe defaults if NVS is corrupted

## Communication

| Link | Protocol | Notes |
|---|---|---|
| S3-A ↔ S3-B | UART | 115,200 baud, cross-validation of measurements |
| Module → Gateway | ESP-NOW | ~1 Mbps, <5 ms typical latency, 250-byte max payload |
| Gateway → Supervision app | Wi-Fi (AP+STA) + TCP (port 8080) | Gateway hosts its own AP (WPA2-PSK, static IP) |
| App ↔ Gateway (live data) | WebSocket | Persistent bidirectional link, avoids HTTP polling overhead |
| Inter-module pairing | XTEA challenge-response (32 rounds) | 4×32-bit shared key; frames use CRC-16/CCITT-FALSE for integrity |

## Protection algorithms

Every protection runs at 5 Hz (electrical) or 1 Hz (thermal/environmental) with two response tiers — an **alert** threshold (operator notification only) and a **protection** threshold (immediate contactor trip) — to avoid nuisance trips on benign transients.

| Parameter | Alert | Protection |
|---|---|---|
| RMS voltage (per phase) | ±10% V_nom | ±15% V_nom |
| RMS current (per phase) | 110% I_nom | 120% I_nom |
| Phase loss | V < 50% V_nom | V < 15% for >100 ms |
| Voltage imbalance (NEMA MG-1) | >2% | >5% |
| Grid frequency | ±2 Hz | ±5 Hz |
| Thermal model (I²t, 10-min time constant) | 80% | 100% |
| Locked rotor (current + speed combined) | I > 3×I_nom & RPM <10% for >3 s | >5 s |
| Vibration (ISO 10816-aligned) | >0.03 g | >0.1 g / >1.5 g (shock) |
| Temperature | >70 °C | >85 °C |
| Gas (MQ-2 / MQ-4 / MQ-9) | 500 / 2,500 / 35 ppm | 1,000 / 5,000 / 50 ppm |

Notable details: voltage/current RMS computed over a rolling one-cycle (20 ms) window from 860 sps ADS1115 ADCs; phase-sequence reversal is treated as an immediate critical fault (would drive the motor in reverse); gas sensors are held out of service for a mandatory 120 s preheat after power-up.

## Local HMI

20×4 character LCD (via PCF8574 I²C backpack), 5-button navigation with 20 ms software debounce, 3-tone piezo buzzer (confirm / alert / critical), RGB + red status LEDs. 23-screen menu tree spanning live measurements, active/historical faults, threshold configuration, sensor calibration, module pairing, and system diagnostics. Screens refresh every 200 ms; alert values blink, critical values invert.

## Remote supervision app

Browser-based (no native app / App Store dependency), designed in Figma and implemented in HTML5/CSS3/JavaScript, served directly from the gateway's own Wi-Fi access point.

- **Models** — live cards per connected machine, drill-down to full sensor overview, then per-sensor detail (live charts + numeric readout, color-coded green/orange/red), guided "add model" wizard (MAC pairing → protection profile → sensor selection)
- **Analysis** — historical trend charts per sensor, relative-distribution breakdown
- **Database** — browsable storage view per machine, with per-record status/timestamp tables
- **Forecasts** — upcoming predicted-fault cards (date, description, fault type, affected machine)
- **Settings** — global system settings; per-machine protection profile (Low / Medium / High); configurable email alerting with editable message text

## Cloud & data layer

Firebase Realtime Database was chosen deliberately over on-device storage: the ESP32's onboard memory can't hold meaningful long-term history, whereas Firebase gives effectively unlimited historization, automatic real-time sync to every connected client, and a path to future analysis — without which the predictive-maintenance layer below wouldn't be possible.

Telemetry is JSON-structured and timestamped at four rates: fast (5 Hz — voltage/current/RPM/vibration/confidence), slow (1 Hz — temperature/humidity/gas/uptime), system state (0.5 Hz), and fault events (immediate).

## Predictive maintenance

Distinct from the instantaneous threshold protection above: during initial nominal operation, the system learns a "health signature" baseline per machine, then continuously compares new data (e.g., a 24-hour rolling average) against it to catch **slow drift** that never crosses an alert threshold on its own. Examples the system is designed to catch:

- Bearing wear — vibration RMS creeping up ~0.01 g/week while staying under the 0.03 g alert line
- Latent overload — gradual rise in no-load current, suggesting increasing friction or misalignment
- Cooling degradation — operating temperature drifting up ~0.5 °C/day, suggesting a blocked vent

## Safety philosophy

The protection relay defaults to **open** at boot and stays open unless explicitly proven safe. After any fault trip, the system will not restart automatically under any circumstance — an operator must confirm rearm via the local HMI or the supervision app. This is a deliberate industrial-safety requirement, not an oversight: it prevents a faulty machine from silently coming back online unattended.

## Suggested repository structure

```
industrial-protection-system/
├── README.md
├── LICENSE
├── firmware/
│   ├── s3-protection-module/     # ESP32-S3 firmware (ESP-IDF, FreeRTOS)
│   └── gateway-module/           # ESP32-WROOM-32 gateway firmware
├── hardware/
│   ├── schematics/                # Eagle/KiCad sources + exported PDFs
│   ├── pcb/                       # Gerbers, BOM
│   └── datasheets/
├── supervision-app/
│   └── web/                       # HTML/CSS/JS browser-based supervision UI
├── docs/
│   ├── memoire.pdf                 # Full academic thesis (French)
│   └── figures/                    # System diagrams, screenshots
└── .github/                        # optional: CI, issue templates
```

## Build & flash (ESP-IDF)

```bash
# Install VS Code + the Espressif IDF extension
# (auto-installs ESP-IDF, Xtensa GCC toolchain, CMake/Ninja)
idf.py set-target esp32s3   # or esp32, for the gateway module
idf.py menuconfig           # project configuration
idf.py build
idf.py flash monitor
```

## Academic context

- **Diploma:** Brevet de Technicien Supérieur (BTS), Industrial Electronics — INSFP Kacem Cherif, Sétif, Algeria (April 2026)
- **Developed during:** a technical internship at Sonelgaz's Aïn Arnat combined-cycle power plant (SPE, 1,015.121 MW)
- **Co-developed by:** Aymane Rehahla and El Meliani Mohammed
- **Supervised by:** Professeur Kharshi Samia

## License

Not yet specified — add a LICENSE file appropriate to how you intend to share this (MIT is a common permissive default for portfolio firmware projects).
