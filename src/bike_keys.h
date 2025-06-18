#pragma once
enum BIKE_KEYS
{
    BK_WHEEL_UP,
    BK_WHEEL_DOWN,
    BK_WHEEL_LEFT,
    BK_WHEEL_RIGHT,
    BK_REPEATER_LEFT,
    BK_REPEATER_RIGHT,
    BK_REPEATER_CENTER,
    BK_ABS,
    BK_ESA,
    BK_MAX
};

enum BIKE_KEY_STATUS
{
    BK_STATUS_RELEASED,
    BK_STATUS_PRESSED,
    BK_STATUS_MAX
};

#define BK_LONG_KEY_PRESS_TIME 1000
#define NOT_DEFINED_KEY 99
class __BIKE_KEY
{

public:
    int pin;
    long ms_press;
    long ms_release;
    BIKE_KEY_STATUS status;
    uint8_t short_key;
    uint8_t long_key;

    __BIKE_KEY(int pin, uint8_t short_key, uint8_t long_key)
    {
        this->pin = pin;
        this->ms_press = 0;
        this->ms_release = 0;
        this->status = BK_STATUS_RELEASED;
        this->short_key = short_key;
        this->long_key = long_key;
    }

    __BIKE_KEY(int pin, uint8_t short_key)
    {
        this->pin = pin;
        this->ms_press = 0;
        this->ms_release = 0;
        this->status = BK_STATUS_RELEASED;
        this->short_key = short_key;
        this->long_key = 0;
    }

    void press()
    {
        if (this->status == BK_STATUS_PRESSED)
            return;
        this->ms_press = millis();
        this->status = BK_STATUS_PRESSED;
    }

    bool check_long_key()
    {
        if (this->status == BK_STATUS_PRESSED)
        {
            if (millis() - this->ms_press > BK_LONG_KEY_PRESS_TIME)
            {
                this->ms_release = millis();
                this->status = BK_STATUS_RELEASED;
                return true;
            }
        }
        return false;
    }

    uint8_t release()
    {
        if (this->status == BK_STATUS_PRESSED)
        {
            this->ms_release = millis();
            this->status = BK_STATUS_RELEASED;

            return this->short_key;
        }

        return 0;
    }
};