#pragma once

#include <Arduino.h>
#include <vector>
enum _LED_STATE
{
    LED_OFF = 0x0000,
    LED_LONG_OFF = 0x0001,
    LED_2TIME_ON = 0x0005,
    LED_BLINK_SMALL_ON = 0x0003,
    LED_BLINK_SLOW = 0x00FF,
    LED_BLINK_FAST = 0xF0F0,
    LED_BLINK_VERY_FAST = 0xCCCC,
    LED_BLINK_SUPER_FAST = 0xAAAA,
    LED_ON = 0xFFFF,
};

typedef struct
{
    int pin;
    String name;
    _LED_STATE state;
    bool invert;
    bool use_pwm;
    int current_state_index;
    int pwm_value;
    int off_value;
    int on_value;
} MSYS_LED_T;

class MSYS_LED
{
private:
    std::vector<MSYS_LED_T> leds;
    MSYS_LED_T* getLED(String name);
    bool is_thread_running = false;

public:
    MSYS_LED();
    ~MSYS_LED();
    void addLED(String name, int pin, bool invert = false, bool use_pwm = false);
    void removeLED(String name);
    void setLEDCalibrationValue(String name, int off_value, int on_value);
    void setLEDState(String name, _LED_STATE state);
    void getLEDState(String name);

    static void threadLED(void *args);
};