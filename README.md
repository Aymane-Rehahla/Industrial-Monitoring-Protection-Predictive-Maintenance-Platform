# Industrial Monitoring & Protection Platform
### Safety-Critical Embedded System for Three-Phase Industrial Equipment

> ESP32-S3 • ESP32 • ESP-IDF • Embedded C • Industrial Electronics • Functional Safety • Remote Monitoring

---

## Overview

This project is my graduation project and represents the largest embedded system I have developed.

The goal was not simply to read sensors, but to design a **fault-tolerant industrial protection platform** capable of monitoring electrical and environmental conditions while remaining safe even if software or hardware partially fails.

The system combines multiple microcontrollers, redundant supervision, hardware watchdogs, modular sensor architecture, remote monitoring, and industrial protection logic.

Unlike many academic projects, this system was designed from an engineering perspective:

> **Never trust software alone. Every critical decision must be independently verified.**

---

# System Architecture

```
                     ┌───────────────────────┐
                     │      iPad HMI         │
                     │ Web Dashboard         │
                     │ Remote Monitoring     │
                     └──────────┬────────────┘
                                │
                           WiFi / TCP
                                │
                     ┌──────────▼───────────┐
                     │      ESP32 Gateway    │
                     │ Communication Bridge  │
                     │ Web Server            │
                     │ ESP-NOW               │
                     └──────────┬────────────┘
                                │
                          ESP-NOW Network
                                │
              ┌─────────────────┴─────────────────┐
              │                                   │
      ESP32-S3 Node A                     ESP32-S3 Node B
      Industrial Protection               Industrial Protection
              │                                   │
              └──────────────┬────────────────────┘
                             │
                   Dual ATtiny85 Watchdogs
                             │
                          NE555 Supervisor
                             │
                    Safety Relay / Contactor
```

---

# Main Features

- Three-phase electrical monitoring
- Industrial protection system
- Modular sensor architecture
- Remote web dashboard
- ESP-NOW communication
- Configurable protection thresholds
- Dual-node redundancy
- Multi-layer watchdog architecture
- Event logging
- Expandable hardware design

---

# Engineering Problems Solved

## 1. Software can fail

A normal embedded project assumes the firmware is always running correctly.

I assumed the opposite.

The protection system was designed so that **hardware can still force the system into a safe state even if firmware crashes or behaves unexpectedly.**

---

## 2. Multi-Layer Safety Supervision

Instead of relying on a single watchdog timer, I designed multiple independent protection layers.

### Layer 1 — ESP32 Cross Monitoring

Two ESP32 controllers continuously monitor each other's state.

If one controller detects abnormal behavior from the other, it refuses unsafe operation.

---

### Layer 2 — ATtiny85 Hardware Watchdogs

Each ESP32 communicates with an independent ATtiny85.

Instead of sending a fixed heartbeat, the ESP32 continuously transmits a **rolling key**.

The watchdog validates the changing sequence.

This prevents situations where:

- firmware freezes
- communication glitches
- repeated heartbeat values
- partially corrupted execution

A frozen processor cannot fake a valid rolling key.

---

### Layer 3 — Independent NE555 Supervisor

The final safety layer contains no firmware at all.

A simple NE555 timer expects periodic pulses.

If every controller fails to generate the required pulse:

- relay power is removed
- contactor opens
- processors are reset

This guarantees a safe fallback even during catastrophic firmware failure.

---

# Contactor Verification

One lesson I learned during my industrial internship was:

> Never assume a contactor actually changed state.

To verify operation, I placed:

- three voltage sensors before the contactor
- three voltage sensors after the contactor

The firmware compares both measurements.

This allows detection of:

- welded contacts
- contactor failing to close
- contactor failing to open
- unexpected phase behavior

Instead of trusting the relay command, the system verifies the physical electrical result.

---

# Electrical Measurements

The system monitors:

- Three-phase voltage
- Three-phase current
- Frequency
- Phase sequence
- Phase loss
- Voltage imbalance
- Current imbalance

Environmental sensors include:

- Temperature
- Humidity
- Gas detection
- Vibration
- RPM

---

# ADC Sampling Challenge

One practical engineering challenge involved the ADC sampling rate.

The available ADC bandwidth was insufficient to continuously sample every phase independently at full resolution.

Instead of accepting reduced accuracy, I designed a sampling strategy that reconstructs each phase waveform while distributing the available samples across all three phases.

This allows simultaneous monitoring of the complete electrical system using limited ADC resources.

---

# Flexible Industrial Configuration

Industrial installations are not identical.

To support multiple environments, the firmware allows:

- Three-phase configuration
- Three-phase + Neutral configuration
- Configurable sensor limits
- Modular sensor enable/disable
- Expandable hardware drivers

Adding or removing sensors requires minimal software modification.

---

# Safety Philosophy

The project follows one simple rule:

> **Never trust a single source of information.**

Critical decisions are verified using:

- redundant controllers
- redundant watchdogs
- electrical feedback
- independent hardware supervision

Every protection layer assumes another layer may fail.

---

# Technologies

## Firmware

- ESP-IDF
- Embedded C
- FreeRTOS
- ESP-NOW
- WiFi

## Hardware

- ESP32-S3
- ESP32-WROOM
- ADS1115
- ATtiny85
- NE555
- Relay feedback circuitry
- Modular sensor interfaces

---

# Lessons Learned

Developing this project taught me considerably more than programming microcontrollers.

It required understanding:

- industrial safety principles
- fault tolerance
- hardware/software co-design
- communication reliability
- embedded architecture
- measurement systems
- redundancy
- practical engineering trade-offs

Many design decisions were inspired by observations made during my internship at a combined-cycle power plant, where verification and fail-safe operation are more important than simply making a system work.

---

# Current Status

The platform is functional and continues to evolve.

Planned improvements include:

- complete protection algorithms
- advanced diagnostics
- additional industrial communication protocols
- hardware validation
- extended testing

---

# About Me

I'm an Industrial Electronics graduate from Algeria interested in:

- Embedded Software
- Firmware Engineering
- Industrial Automation
- Safety-Critical Systems
- Electronics Design
- Test Engineering

I'm currently looking for opportunities involving **embedded systems and industrial automation**, including positions offering **visa sponsorship**.

---

## Contact

📧 Aymen.rehahla007@gmail.com

LinkedIn:
https://www.linkedin.com/in/aymen-rehahla-907391335/

GitHub:
https://github.com/Aymane-Rehahla
