# BMW Motorrad Cluster — ESP32 펌웨어

[English](README.md)

BMW Motorrad **LIN** 신호를 스니핑해 BLE로 차량 상태를 알리고, Wonder Wheel / 핸들 조작을 **BLE HID 키보드**로 보내는 ESP32 / ESP32-S3 펌웨어입니다.

| 저장소 | 역할 |
|---|---|
| [bmw-motorrad-cluster-linbus](https://github.com/kendrickkim/bmw-motorrad-cluster-linbus) | LIN 디코드 표 (프로토콜 기준) |
| **이 저장소** (`bmw-motorrad-cluster-esp32`) | LIN 스니프 → BLE 텔레메트리 + HID |
| [bmw-motorrad-cluster-application](https://github.com/kendrickkim/bmw-motorrad-cluster-application) | Android Compose 클러스터 UI |

## 기능

- 소프트웨어 LIN 수신 (`SoftwareLin`), 기본 **9600 baud**
- Message ID 파싱: `0x14`(컨트롤/Wonder Wheel), `0x20`(이그니션/휠 RPM), `0xE2`(RPM/기어), 전압 관련 프레임 등
- BLE 장치명 `BMW-WWC-<MAC 접미사>`
- 차량 상태 notify 페이로드 (`CAR_DATA_LENGTH` = 15바이트)
- Wonder Wheel / 버튼용 BLE HID 키보드 (숏·롱 프레스)
- 선택적 SSD1306 OLED
- 바이크 없이 벤치 테스트: `ON_BIKE` 해제 후 GPIO 테스트 버튼

## 구조

```text
Bike LIN ──► LIN 트랜시버 (예: TJA1021) ──► ESP32
                                                  │
                          ┌───────────────────────┼───────────────────────┐
                          ▼                       ▼                       ▼
                   프레임 파싱              car_status 패킹          HID 키 리포트
                          │                       │                       │
                          └─────────── BLE (Device Info notify + HID) ───┘
                                              │
                                              ▼
                                    Android 클러스터 앱
```

## 하드웨어

| 항목 | 설명 |
|---|---|
| MCU | ESP32 또는 **ESP32-S3** (기본 보드: `esp32-s3-bmw-wonderwheel`) |
| LIN 트랜시버 | TJA1021 계열 권장 |
| OLED (선택) | SSD1306 128×32 I2C (`0x3C`) |
| 스마트폰 | 클러스터 앱 + BLE |

### 기본 GPIO (`src/main.cpp`)

| 신호 | GPIO |
|---|---|
| LIN RX | 16 |
| LIN TX | 17 |
| LIN SLEEP | 18 |
| LIN commander | 8 |
| OLED SDA / SCL | 7 / 15 |
| 상태 LED | 35, 36, 37 |
| Boot / ESC 버튼 | 0 |

핀맵은 `boards/` 커스텀 보드 정의를 따릅니다. PCB에 맞게 수정하세요.

## BLE 인터페이스

| 항목 | 값 |
|---|---|
| 장치명 | `BMW-WWC-` + BT MAC 뒤 8자 |
| HID | 키보드 Appearance, manufacturer `BMW-WWC` |
| 텔레메트리 | Device Information `0x180A`, characteristic `0x2A01` (read + notify) |
| 본딩 | `ESP_LE_AUTH_BOND` |

### 차량 상태 페이로드 (`buildData`)

| Offset | 필드 |
|:------:|------|
| 0 | 버튼 (하위 바이트) |
| 1 | 버튼 (상위 바이트) |
| 2 | Wonder Wheel 값 |
| 3 | 기어 |
| 4 | 전압 (raw) |
| 5–6 | 휠 RPM (LE uint16) |
| 7–8 | 엔진 RPM (LE uint16) |
| 9 | 이그니션 (`0` / `1`) |
| 10–14 | 예약 / 0 |

롱프레스 기준: **1000 ms**.

## 빌드·업로드

[PlatformIO](https://platformio.org/) 필요.

```bash
cd bmw-motorrad-cluster-esp32
pio run -e esp32-bmw-wonderwheel
pio run -e esp32-bmw-wonderwheel -t upload
pio device monitor -b 115200
```

`platformio.ini`에서 포트(`upload_port` / `monitor_port`)를 환경에 맞게 바꾸세요.

- **실차:** `main.cpp`의 `#define ON_BIKE` 유지
- **벤치:** `ON_BIKE`를 끄면 테스트 GPIO 버튼 활성화

## 저장소 구성

```text
bmw-motorrad-cluster-esp32/
├── platformio.ini
├── boards/
├── esp32_partition_*.csv
└── src/
    ├── main.cpp
    ├── SoftwareLin.*
    ├── car_status.*
    ├── ww_bluetooth.*
    ├── bike_keys.h
    └── ...
```

## 프로토콜 참고

[linbus LIN_ANALYSIS.md](https://github.com/kendrickkim/bmw-motorrad-cluster-linbus/blob/main/lin_analysis/LIN_ANALYSIS.md)  
펌웨어와 분석 문서가 다르면 **둘 다** 맞추고 차종·연식을 남기세요.

## 안전

12V / LIN DIY는 바이크·보드 손상을 일으킬 수 있습니다. 적절한 트랜시버·공통 GND·퓨즈를 사용하세요. 이 펌웨어는 폰 UI용 미러링용이며 차량 안전 장치를 우회하는 용도가 아닙니다.
