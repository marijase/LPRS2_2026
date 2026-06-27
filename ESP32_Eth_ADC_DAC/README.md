# Web-Controlled Stereo Signal Generator with Real-Time Signal Analysis

**Author:** Marija Sekulić
**Date:** June 2026

---

## About the project

The system generates digital audio signals of different waveforms (sine, square, triangle) on an ESP32-ETH01 microcontroller, converts them to analog signals using a PCM5102A digital-to-analog converter (DAC), then loops them back into the system through a WM8782S analog-to-digital converter (ADC) where they are analyzed in real time.

The user controls signal parameters — waveform, frequency, and amplitude — through a web application accessible over Ethernet or WiFi. The system displays detected frequency, RMS amplitude, and a real-time oscillogram of the received signal.

---

## Hardware

| Component | Role |
|---|---|
| **ESP32-ETH01** | Microcontroller with Ethernet and WiFi |
| **WM8782S** | 24-bit stereo audio ADC |
| **PCM5102A** | 32-bit stereo audio DAC |
| **USB-TTL adapter** | For firmware upload and serial monitoring |

---

## Features

- Real-time stereo audio signal generation (sine, square, triangle)
- 48 kHz sample rate, 16-bit resolution
- Independent control of left and right channels (frequency, amplitude, mute)
- RMS amplitude calculation
- Real-time signal oscillogram (10 updates per second)
- Responsive web application
- Dual network mode: Ethernet (primary) + WiFi AP (fallback)

---

## Repository structure

```
Projekat/
│
├── HW/                                Hardware
│   └── ESP32_Eth_ADC_DAC/             KiCad project (schematic + PCB)
│       ├── Libs/                      Custom KiCad libraries
│       │   └── ESP32_Eth_ADC_DAC.pretty/  Component footprints
│       ├── ESP32_Eth_ADC_DAC.kicad_pro
│       ├── ESP32_Eth_ADC_DAC.kicad_sch    Electrical schematic
│       ├── ESP32_Eth_ADC_DAC.kicad_pcb    PCB layout
│       └── ...
│
├── SW/                                Software
│   ├── signal_generator/              Main firmware
│   │   ├── signal_generator.ino       Arduino code
│   │   ├── webpage.h                  Web application (HTML/CSS/JS)
│   │   └── README.md
│   │
│   └── blinkanje/                     Test code (blink LED)
│       └── blinkanje.ino
│
└── docs/                              Documentation
    ├── datasheets/                    Component datasheets
    │   ├── ESP32_datasheet_en.pdf
    │   ├── WM8782_datasheet.pdf
    │   ├── WT32-ETH01_datasheet_V1.4_en.pdf
    │   ├── WT32-ETH01_EVO_Datasheet_V2.0_EN.pdf
    │   └── wt32-eth01-v1-4-serial-to-ethernet-module.pdf
    │
    └── dokumentacija.pdf    Main project documentation
```

---

## Technologies used

### Hardware protocols
- **I²S (Inter-IC Sound)** — digital audio protocol between microcontroller and audio chips
- **UART** — serial communication for firmware upload and debugging
- **RMII** — interface between ESP32 and LAN8720A Ethernet PHY chip

### Network protocols
- **Ethernet 10/100 Mbps** — via LAN8720A PHY
- **WiFi 802.11 b/g/n** — in Access Point mode (fallback)
- **HTTP** — web server for serving the page
- **WebSocket** — real-time bidirectional communication

### Software tools
- **Arduino IDE** with ESP32 board package v3.3.8 (Espressif)
- **KiCad** EDA suite for schematic and PCB
- **C/C++** — firmware
- **HTML / CSS / vanilla JavaScript** — web application
- **Chart.js** — library for signal visualization

---

## Pin mapping

### I²S TX (ESP32 master → DAC slave)
| ESP32 pin | DAC pin | Signal |
|---|---|---|
| IO15 (pin 20) | Pin 2 (BCK) | Bit clock |
| IO14 (pin 19) | Pin 4 (LCK) | LR clock |
| IO4 (pin 16) | Pin 3 (DIN) | Audio data |

### I²S RX (ADC master → ESP32 slave)
| ADC pin | ESP32 pin | Signal |
|---|---|---|
| Pin 1 (B) | IO36 (pin 21) | Bit clock |
| Pin 2 (LR) | IO39 (pin 22) | LR clock |
| Pin 3 (D) | IO35 (pin 17) | Audio data |

### Audio loopback
| From | To |
|---|---|
| DAC pin 7 (LROUT) | ADC pin 9 (Lin) |
| DAC pin 9 (ROUT) | ADC pin 7 (Rin) |

---

## Getting started

### 1. Hardware assembly

Connect the components according to the electrical schematic in `HW/ESP32_Eth_ADC_DAC/`. Detailed instructions and connection tables are available in the main documentation.

### 2. ADC module configuration

Set the DIP switch on the WM8782S module:

| Switch | Position | Function |
|---|---|---|
| 1 (M/S) | + | Master mode |
| 2 (Format) | 0 | I²S format |
| 3 (Sample rate) | − | 48 kHz |
| 4 (Bits) | − | 16-bit |

### 3. Development environment setup

1. Install **Arduino IDE** (from arduino.cc)
2. Add ESP32 board package:
   - File → Preferences → Additional Boards Manager URLs:
     `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
   - Tools → Board → Boards Manager → install **esp32** (v3.3.8)
3. Install **WebSockets** library (Markus Sattler) via Library Manager

### 4. Arduino IDE configuration

- Tools → Board → esp32 → **WT32-ETH01 Ethernet Module**
- Upload Speed: **115200**
- Flash Frequency: **80MHz**
- Flash Mode: **QIO**
- Partition Scheme: **Default 4MB with spiffs**

### 5. Firmware upload

1. Unplug USB-TTL from the laptop
2. Connect ESP32 IO0 (pin 24) to GND (boot mode)
3. Plug in USB-TTL
4. Open `SW/signal_generator/signal_generator.ino`
5. Click **Upload**
6. During "Connecting...." unplug and plug EN pin back
7. After upload: unplug USB, remove IO0 from GND, plug USB back

### 6. Accessing the web application

**Via Ethernet cable (recommended):**
1. Connect a UTP cable from the ESP32 RJ45 to a free LAN port on the router
2. Open Serial Monitor (115200 baud) and read the IP address
3. Open that IP address in a browser

**Via WiFi AP (fallback):**
1. Connect to the WiFi network **ESP32-Signal-Generator** (password: `12345678`)
2. Open `http://192.168.4.1` in a browser

---

## Technical details

### I²S configuration

The ESP32 has two independent I²S controllers:
- **I²S0** — master TX channel to the DAC (generates BCK and LRCK)
- **I²S1** — slave RX channel from the ADC (receives BCK and LRCK from the ADC)

Since the WM8782S ADC is configured as master (using its onboard 24.576 MHz and 22.5792 MHz crystals), and the PCM5102A DAC is always a slave, using both ESP32 I²S controllers is required.

### Signal generation

Audio samples are computed in software, sample by sample (48000 times per second per channel), in the main loop. For each sample the current signal phase is calculated and converted to a 16-bit signed integer, which is written to the I²S DMA buffer.

### Signal analysis

- **RMS amplitude:** square root of the mean of squared samples in the buffer
- **Frequency:** zero-crossing detection (number of negative-to-positive transitions divided by buffer duration)

### WebSocket communication

Bidirectional real-time communication on port 81:
- **ESP32 → client:** 10 updates per second with JSON payload (detected frequency, RMS, array of 128 samples for oscillogram)
- **Client → ESP32:** commands on every slider move or button click

---

## Demonstration

The system supports the following features for demonstration:

- **Stereo signal generation** — different frequencies on L and R channels simultaneously
- **Real-time waveform switching** — sine, square, triangle
- **Independent channel muting**
- **Audio output via 3.5mm jack** — direct headphone connection
- **Real-time visualization** — oscillogram of the received signal
- **Dual-mode networking** — automatic fallback from Ethernet to WiFi AP

---

## License
...

---

# Youtube video:
[text](https://youtu.be/xQKN2IF4LF8?si=VLTadd05omXWZXxb)
