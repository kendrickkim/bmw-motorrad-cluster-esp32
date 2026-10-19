# BMW Motorrad Cluster — ESP32 Firmware

[한국어](README-kr.md)

ESP32 / ESP32-S3 firmware that sniffs BMW Motorrad **LIN** traffic, forwards each frame unchanged over BLE, and turns Message ID `0x14` into a **BLE HID keyboard**.

| Repository | Role |
|---|---|
| [bmw-motorrad-cluster-linbus](https://github.com/kendrickkim/bmw-motorrad-cluster-linbus) | LIN decode tables (protocol source of truth) |
| **This repo** (`bmw-motorrad-cluster-esp32`) | LIN sniff → BLE telemetry + HID |
| [bmw-motorrad-cluster-application](https://github.com/kendrickkim/bmw-motorrad-cluster-application) | Android Compose cluster UI |

## Features

- Software LIN reader (`SoftwareLin`) at **9600 baud** (optional autobaud path in code)
- Forward every LIN frame as Message ID + Data-0..Data-5 (7 bytes). `0x14` is also decoded locally into HID keys
- BLE peripheral named `BMW-WWC-<MAC suffix>`
- Notify characteristic `0x2A01`. Identical repeats of the same ID are not sent again
- BLE HID keyboard for wheel / button shortcuts (short + long press)
- Optional SSD1306 OLED status (`BMW Motorrad` splash)
- Bench mode without a bike: undefine `ON_BIKE` and use GPIO test buttons

## Architecture

```text
Bike LIN ──► LIN transceiver (e.g. TJA1021) ──► ESP32
                                                  │
                          ┌───────────────────────┼───────────────────────┐
                          ▼                       ▼                       ▼
                   raw 7-byte frame        ID 0x14 → keys          HID key reports
                          │                       │                       │
                          └─────────── BLE (Device Info notify + HID) ───┘
                                              │
                                              ▼
                                    Android cluster app
```

## Hardware

| Item | Notes |
|---|---|
| MCU | ESP32 or **ESP32-S3** (default PlatformIO board: custom `esp32-s3-bmw-wonderwheel`) |
| LIN transceiver | TJA1021-class module recommended |
| OLED (optional) | SSD1306 128×32 I2C (`0x3C`) |
| Phone | Runs the cluster application over BLE |

### Default GPIO (see `src/main.cpp`)

| Signal | GPIO |
|---|---|
| LIN RX | 16 |
| LIN TX | 17 |
| LIN SLEEP | 18 |
| LIN commander | 8 |
| OLED SDA / SCL | 7 / 15 |
| Status LEDs | 35, 36, 37 |
| Boot / LIN capture | 0 |

Pin maps follow the custom board JSON under `boards/`. Adjust for your PCB.

## BLE interface

| Item | Value |
|---|---|
| Device name | `BMW-WWC-` + last 8 characters of BT MAC |
| HID | Keyboard appearance; manufacturer string `BMW-WWC` |
| Telemetry | Device Information service `0x180A`, characteristic `0x2A01` (read + notify) |
| Bonding | `ESP_LE_AUTH_BOND` |

### LIN frame notify (on the bike)

Byte indexes match `bmw-motorrad-cluster-linbus` `lin_analysis`. The checksum is not included. The same layout is sent for every ID, including `0x14`.

| Offset | Field |
|:------:|-------|
| 0 | Message ID |
| 1 | Data-0 |
| 2 | Data-1 |
| 3 | Data-2 |
| 4 | Data-3 |
| 5 | Data-4 |
| 6 | Data-5 |

`0x14` keyboard uses Data-1 (left handle), Data-3 (Wonder Wheel in/out), and Data-5 (rotation). Other IDs are not decoded in this firmware.

Bench mode (`ON_BIKE` undefined) has no LIN frames, so it still notifies the packed `car_status` blob (`CAR_DATA_LENGTH` = 15).

Pressing GPIO0 replays the embedded `lin_capture.txt` as the same 7-byte notify, one line every 10 ms. Pressing again during playback restarts from the first line. This replay does not emit the `0x14` keyboard.

HID mapping examples (LIN path): Wonder Wheel In/Out → arrows / ESC / numpad; wheel rotation → up/down arrows. Long-press threshold: **1000 ms** (`BK_LONG_KEY_PRESS_TIME`).

## Build & flash

Requirements: [PlatformIO](https://platformio.org/) (VS Code / CLI).

```bash
cd bmw-motorrad-cluster-esp32
pio run -e esp32-bmw-wonderwheel
pio run -e esp32-bmw-wonderwheel -t upload
pio device monitor -b 115200
```

Configuration lives in `platformio.ini`:

- Framework: Arduino on Espressif32
- Partition table: `esp32_partition_64mbit_bmw_wonderwheel.csv`
- Library: ThingPulse SSD1306 OLED driver
- Set `upload_port` / `monitor_port` for your machine (defaults may point at `COM7`)

### On-bike vs bench

- **On bike:** keep `#define ON_BIKE` in `main.cpp` (default).
- **Bench:** comment out `ON_BIKE` to enable GPIO test buttons for wheel simulation.

## Repository layout

```text
bmw-motorrad-cluster-esp32/
├── platformio.ini
├── boards/                 # Custom ESP32 / ESP32-S3 board defs
├── esp32_partition_*.csv
└── src/
    ├── main.cpp            # LIN loop, key map, FreeRTOS tasks
    ├── SoftwareLin.*       # Software LIN
    ├── car_status.*        # Packed telemetry
    ├── ww_bluetooth.*      # BLE HID + notify
    ├── bike_keys.h         # Short / long key state machine
    ├── common.*
    ├── logger/
    └── espsoftwareserial/  # Vendored soft serial
```

## Protocol reference

Decode formulas and Message IDs: [linbus LIN_ANALYSIS.md](https://github.com/kendrickkim/bmw-motorrad-cluster-linbus/blob/main/lin_analysis/LIN_ANALYSIS.md).

If firmware masks/formulas diverge from the analysis doc, update **both** and note the bike model/year.

## Safety

DIY 12 V / LIN work can damage the bike or ESP32. Use a proper transceiver, common ground, and fused power. This firmware sniffs and mirrors controls for a phone UI — do not use it to defeat vehicle safety systems.
