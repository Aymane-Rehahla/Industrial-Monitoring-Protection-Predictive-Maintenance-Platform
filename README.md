# Industrial Distributed Monitoring & Protection Platform

> A modular industrial monitoring and protection system built with ESP32-S3 and ESP32 using ESP-IDF.
>
> Designed for reliability, fault tolerance, and safety-critical industrial environments.

---

## Overview

This project is my graduation project and represents the largest embedded system I have designed.

The system monitors industrial electrical and environmental parameters in real time while protecting equipment through a layered safety architecture.

Unlike many academic projects, this system was designed with **industrial reliability** as the primary goal.

It combines:

- Dual ESP32 architecture
- Independent hardware watchdogs
- Redundant communication
- Modular sensor framework
- Industrial HMI
- Remote monitoring dashboard
- Real-time protection algorithms

---

# Architecture

```
                   iPad Dashboard
                  (Web Application)
                         │
                    WiFi / TCP
                         │
                 ESP32 HMI Gateway
                         │
                  Encrypted ESP-NOW
                         │
      ┌──────────────────┴──────────────────┐
      │                                     │
 ESP32-S3 Safety Node A           ESP32-S3 Safety Node B
      │                                     │
      └────────── Cross Validation ─────────┘
                     │
              Dual ATtiny85 Watchdogs
                     │
               Independent NE555 Watchdog
                     │
               Industrial Safety Relay
```

---

# Key Features

## Industrial Protection

- Three-phase voltage monitoring
- Three-phase current monitoring
- Temperature monitoring
- Humidity monitoring
- Gas detection
- RPM monitoring
- Vibration monitoring
- Relay feedback verification
- Fault logging
- Alarm management

---

## Real-Time Processing

The ESP32-S3 performs all protection-critical calculations:

- RMS calculation
- Phase sequence detection
- Phase loss detection
- Voltage imbalance
- Current imbalance
- Frequency measurement
- Locked rotor detection
- Thermal I²t model
- Shock detection
- Safety trips

The ADC hardware provides roughly 860 samples/second in total — not per phase. Reconstructing an accurate current waveform for all three phases at once meant that budget had to be shared rather than used sequentially: the sample rate is divided across the three phases so each one is still captured within the same electrical cycle, instead of measuring one phase fully before moving to the next and introducing timing skew between them.

If a condition can damage equipment...

**the ESP32 reacts immediately.**

---

## Remote Monitoring

The ESP32 Gateway serves a web application accessible from an iPad or any browser.

Features include

- Live dashboard
- Real-time sensor values
- Alarm history
- Historical trends
- Waveform visualization
- Configuration interface
- Maintenance analytics

---

# Safety Philosophy

The most important design goal was **eliminating single points of failure.**

Every protection layer assumes another layer may eventually fail.

## Layer 1 — Software

Industrial state machines

Fault handling

CRC validation

Communication verification

Configuration validation

---

## Layer 2 — Dual ESP32 Supervision

Two ESP32 controllers continuously supervise each other.

Each verifies

- communication integrity
- sensor validity
- heartbeat
- operating state

If disagreement occurs

the system enters a safe state.

---

## Layer 3 — Intelligent Hardware Watchdogs

Two ATtiny85 microcontrollers independently supervise the ESP32 controllers.

Instead of accepting a simple heartbeat,

the watchdog expects a **rolling authentication key**.

This prevents situations where a crashed processor repeatedly outputs the same pulse.

If the rolling key becomes invalid,

the watchdog assumes the firmware is malfunctioning and removes the safety relay.

---

## Layer 4 — Independent NE555 Emergency Watchdog

Even watchdogs can fail.

To eliminate this possibility, an entirely independent NE555 hardware watchdog was added.

Unlike the programmable watchdogs,

the NE555 requires no firmware.

If every controller becomes unresponsive and no valid heartbeat reaches the timer,

the NE555 automatically

- opens the safety contactor
- resets the processors
- forces the system into a safe state

This final layer is completely hardware-based and cannot be affected by firmware bugs.

---

# Contactor State Verification

Commanding the contactor to open is not the same as confirming it actually did.

Under fault current, contactor contacts can weld or stick closed — silently defeating every protection layer above while still reporting a "safe" command was issued.

To catch this, voltage and current are sensed independently on both sides of the contactor: three points upstream (supply side) and three points downstream (load side), one per phase. Comparing the two sides directly reveals the real physical state of the contactor — whether it failed to open when commanded, or is welded shut — rather than relying on the command signal alone.

---

# Modular Sensor Architecture

One design objective was allowing industrial installations to evolve without modifying firmware.

Sensors are treated as independent modules.

Each module can

- be added
- removed
- enabled
- disabled
- configured

independently.

Protection limits are configurable rather than hardcoded.

A hardware switch also selects between simple (3-phase, no neutral) and complex (3-phase + neutral) wiring configurations, so the same platform can be installed across different machine and service types without a firmware change.

This allows the same firmware to support multiple industrial installations with different requirements.

---

# Communication

## ESP-NOW

- encrypted transport
- CRC validation
- packet sequencing
- custom protocol
- low latency

---

## Gateway

The ESP32 gateway converts embedded telemetry into data suitable for the web dashboard.

---

# Technologies

- ESP-IDF
- C
- FreeRTOS
- ESP32-S3
- ESP32
- ESP-NOW
- TCP/IP
- HTML
- JavaScript
- I2C
- UART
- SPI
- ADS1115
- SHT45
- ATtiny85

---

# Project Highlights

✔ Custom communication protocol

✔ Distributed embedded architecture

✔ Modular driver framework

✔ Industrial HMI

✔ Browser-based dashboard

✔ Hardware redundancy

✔ Layered watchdog system

✔ Safety relay verification

✔ Real-time monitoring

✔ Fault logging

✔ Configurable protection system

---

# Current Status

The platform is fully operational and demonstrates

- embedded firmware
- hardware integration
- communication
- industrial HMI
- remote monitoring

Some advanced algorithms are still under active development as the project continues to evolve.

---

# Future Work

- Advanced predictive diagnostics
- PLC integration (Modbus TCP)
- CAN Bus support
- MQTT gateway
- Cloud analytics
- Machine learning maintenance assistant

---

# About Me

I'm an Industrial Electronics Engineer from Algeria passionate about

- Embedded Systems
- Firmware Development
- Industrial Automation
- Safety-Critical Software

I'm currently seeking Embedded Software, Firmware, Electronics and Industrial Automation opportunities with visa sponsorship.
