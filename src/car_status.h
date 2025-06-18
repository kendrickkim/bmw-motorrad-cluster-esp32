#pragma once
#include <mutex>

#define CAR_DATA_LENGTH 15

class __CAR_STATUS
{

    std::mutex data_mutex;

    bool wheel_value_init = false;
    unsigned short buttons;         // Bitmask for button states
    unsigned char wheel_value = 99; // Value representing the steering wheel position
    unsigned char gear;             // Current gear position 0 : N, 1 : 1, 2 : 2, 3 : 3, 4 : 4, 5 : 5, 6 : 6, 9 : F
    unsigned char voltage;          // Battery voltage in millivolts * 100
    unsigned short speed;           // Speed in km/h
    unsigned short rpm;             // Engine RPM
    unsigned char ignition;         // Ignition state (0: off, 1: on)

    unsigned char car_data[CAR_DATA_LENGTH];

public:
    unsigned char old_car_data[CAR_DATA_LENGTH];

    enum
    {
        BUTTON_LEFT = 1 << 0,
        BUTTON_RIGHT = 1 << 1,
        BUTTON_CENTER = 1 << 2,
        BUTTON_WHEEL_LEFT = 1 << 3,
        BUTTON_WHEEL_RIGHT = 1 << 4,
        BUTTON_WHEEL_UP = 1 << 5,
        BUTTON_WHEEL_DOWN = 1 << 6,
    };

    __CAR_STATUS();
    void setAllButtonRelease();
    void setButtons(unsigned short buttons);
    uint8_t setWheelValue(unsigned char value);
    void setGear(unsigned char gear) { this->gear = gear; }
    void setVoltage(unsigned char voltage) { this->voltage = voltage; }
    void setSpeed(unsigned short speed) { this->speed = speed; }
    void setRpm(unsigned short rpm) { this->rpm = rpm; }
    void setIgnition(unsigned char ignition) { this->ignition = ignition; }

    bool needToUpdate();

    void buildData();

    void printData();
};

extern __CAR_STATUS carStatus;