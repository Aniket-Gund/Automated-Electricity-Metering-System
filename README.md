# ⚡ Automated Electricity Metering System (AEMS) — Smart Grid IoT Edge

[![GitHub Stars](https://img.shields.io/github/stars/Aniket-Gund/Automated-Electricity-Metering-System?style=for-the-badge&color=blue)](https://github.com/Aniket-Gund/Automated-Electricity-Metering-System/stargazers)
[![Hardware](https://img.shields.io/badge/IoT-Smart%20Metering-FF6F00?style=for-the-badge&logo=arduino&logoColor=white)](https://arduino.cc)
[![Edge Computing](https://img.shields.io/badge/Edge%20Computing-LoRa%20%7C%20WiFi%20%7C%20GSM-008080?style=for-the-badge)](https://espressif.com)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=for-the-badge)](LICENSE)

An advanced IoT-driven **Automated Electricity Metering System (AEMS)** engineered for smart power distribution grids. Incorporates bidirectional wireless telemetry (Wi-Fi, GSM, and LoRa), prepaid metering balance updates, automatic power factor correction, and solid-state latching relay load disconnections.

---

## 🌟 Key System Capabilities

- **📊 High-Precision Energy Consumption Monitoring**: Computes real-time kilowatt-hour (kWh) power draw from optical/digital meter pulse outputs.
- **💳 Smart Prepaid Metering & Recharge**: Embedded server endpoints allowing remote account credit recharges and automatic balance deduction.
- **🔌 Remote Cut-off & Reconnection**: Solid-state latching relay controls to remotely isolate non-compliant or zero-balance loads.
- **⚡ Automatic Power Factor Correction (APFC)**: Dynamically adjusts reactive power via capacitor banks to minimize grid transmission losses.
- **🛡️ Tamper & Malfunction Edge Detection**: Implements algorithms to identify phase imbalances, meter bypass tampering, and anomalous loads.

---

## 🏗️ Telemetry & Network Architecture

```mermaid
flowchart TD
    A[Digital Energy Meter Pulse Sensor] --> B[Embedded Edge Microcontroller]
    B --> C[Power Calculation & kWh Accumulator]
    B --> D[Automatic Power Factor Correction Unit]
    C --> E{Connectivity Module}
    E -->|HTTP REST / MQTT| F[Wi-Fi Telemetry Server: WIFI_FINAL.ino]
    E -->|Cellular SMS / GPRS| G[GSM/GPRS Gateway: with_gsm.ino]
    E -->|Long-Range RF| H[LoRa Mesh Network Gateway]
    F --> I[Central Utility Cloud Database & Billing Engine]
    G --> I
    H --> I
    I -->|Disconnect Command on Exhausted Balance| B
    B --> J[Solid-State Latching Relay Isolation]
```

---

## 📂 Repository Contents

```
Automated-Electricity-Metering-System/
│
├── Firmware Sources:
│   ├── WIFI_FINAL.ino              # Wi-Fi web server & telemetry controller
│   ├── with_gsm.ino                # Cellular SIM800 GSM SMS communication
│   ├── GPRS_SERVER_Transfer.ino    # GPRS direct TCP/IP socket data push
│   ├── with_keypad.ino             # Local keypad authentication & recharge
│   ├── simulation_with_all.ino     # Comprehensive simulation test routine
│   └── all_componentlcd_29.ino     # LCD multi-parameter display manager
│
├── Engineering Schematics & PCB:
│   ├── PCB_Final_BOTH.pdf          # Double-sided PCB layout masks
│   ├── PCB_Final_BOTH_With_Silk.pdf# Silkscreen component placement overlay
│   └── Power Requirements And Calculations.pdf
│
└── Research & Documentation:
    ├── AEMS_Report_Final.pdf       # Comprehensive thesis report
    ├── Final Budget.pdf            # Bill of Materials (BOM) & costing
    └── AMS Evaluation-1.pptx       # Technical presentation
```

---

## 🚀 Getting Started

1. **Hardware Assembly**:
   - Refer to `PCB_Final_BOTH_With_Silk.pdf` for PCB soldering and component layout.
   - Connect solid-state latching relay to primary AC power line.
2. **Firmware Selection**:
   - For Wi-Fi telemetry: Open `WIFI_FINAL.ino` in Arduino IDE and configure your SSID and server credentials.
   - For GSM/GPRS: Open `with_gsm.ino` and set utility APN and central SMS receiver number.
3. **Compile & Upload**: Flash the corresponding firmware to your microcontroller board.

---

## 👨‍💻 Author

**Aniket Gund**
- GitHub: [@Aniket-Gund](https://github.com/Aniket-Gund)
- LinkedIn: [Aniket Gund](https://www.linkedin.com/in/aniket-gund/)

---

## 📄 License

Distributed under the [MIT License](LICENSE).
