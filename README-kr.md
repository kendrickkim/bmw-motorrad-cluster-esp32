# BMW Motorrad Cluster — ESP32 펌웨어

[English](README.md)

BMW Motorrad **LIN** 프레임을 해석하지 않고 BLE로 그대로 보내고, Message ID `0x14`만 **BLE HID 키보드**로 만드는 ESP32 / ESP32-S3 펌웨어입니다.

| 저장소 | 역할 |
|---|---|
| [bmw-motorrad-cluster-linbus](https://github.com/kendrickkim/bmw-motorrad-cluster-linbus) | LIN 디코드 표 (프로토콜 기준) |
| **이 저장소** (`bmw-motorrad-cluster-esp32`) | LIN 스니프 → BLE 텔레메트리 + HID |
| [bmw-motorrad-cluster-application](https://github.com/kendrickkim/bmw-motorrad-cluster-application) | Android Compose 클러스터 UI |

## 기능

- 소프트웨어 LIN 수신 (`SoftwareLin`), 기본 **9600 baud**
- LIN 프레임을 Message ID + Data-0..Data-5 (7바이트)로 전달. `0x14`는 같은 프레임으로 HID 키도 생성
- BLE 장치명 `BMW-WWC-<MAC 접미사>`
- notify characteristic `0x2A01`. 같은 ID의 동일 프레임은 다시 보내지 않음
- Wonder Wheel / 버튼용 BLE HID 키보드 (숏·롱 프레스)
- 선택적 SSD1306 OLED
- 바이크 없이 벤치 테스트: `ON_BIKE` 해제 후 GPIO 테스트 버튼

## 구조

```text
Bike LIN ──► LIN 트랜시버 (예: TJA1021) ──► ESP32
                                                  │
                          ┌───────────────────────┼───────────────────────┐
                          ▼                       ▼                       ▼
                   7바이트 원본 프레임       ID 0x14 → 키            HID 키 리포트
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
| Boot / LIN capture | 0 |

핀맵은 `boards/` 커스텀 보드 정의를 따릅니다. PCB에 맞게 수정하세요.

## BLE 인터페이스

| 항목 | 값 |
|---|---|
| 장치명 | `BMW-WWC-` + BT MAC 뒤 8자 |
| HID | 키보드 Appearance, manufacturer `BMW-WWC` |
| 텔레메트리 | Device Information `0x180A`, characteristic `0x2A01` (read + notify) |
| 본딩 | `ESP_LE_AUTH_BOND` |

### LIN 프레임 notify (실차)

바이트 순서는 `bmw-motorrad-cluster-linbus`의 `lin_analysis`와 같습니다. 체크섬은 포함하지 않습니다. `0x14`를 포함한 모든 ID가 같은 형식입니다.

| Offset | 필드 |
|:------:|------|
| 0 | Message ID |
| 1 | Data-0 |
| 2 | Data-1 |
| 3 | Data-2 |
| 4 | Data-3 |
| 5 | Data-4 |
| 6 | Data-5 |

`0x14` 키보드는 Data-1(왼쪽 핸들), Data-3(Wonder Wheel in/out), Data-5(회전값)를 사용합니다. 그 외 ID는 이 펌웨어에서 해석하지 않습니다.

벤치 모드(`ON_BIKE` 해제)에는 LIN 프레임이 없으므로, 기존 `car_status` 15바이트(`CAR_DATA_LENGTH`)를 그대로 notify 합니다.

GPIO0을 누르면 펌웨어에 포함된 `lin_capture.txt`를 한 줄씩 10ms 간격으로 같은 7바이트 notify로 보냅니다. 재생 중 다시 누르면 첫 줄부터 다시 시작합니다. 이 재생은 `0x14` 키보드를 만들지 않습니다.

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
