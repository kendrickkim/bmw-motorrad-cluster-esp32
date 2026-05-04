#include "MSYS_LED.h"
#include "logger/debug_log.h"

MSYS_LED::MSYS_LED()
{
    xTaskCreate(threadLED, "threadLED", 8 * 1024, (void *)this, 1, NULL);
}

MSYS_LED::~MSYS_LED()
{
    is_thread_running = false;
}

void MSYS_LED::setLEDCalibrationValue(String name, int off_value, int on_value)
{
    MSYS_LED_T *led = getLED(name);
    if (led != NULL)
    {
        led->off_value = off_value;
        led->on_value = on_value;
    }
}

void MSYS_LED::addLED(String name, int pin, bool invert, bool use_pwm)
{
    MSYS_LED_T led = {pin, name, LED_OFF, invert, use_pwm, 0, 0, 0, 255};
    if (use_pwm)
    {
        led.off_value = invert ? 255 : 0;
        led.on_value = invert ? 0 : 255;
        analogWrite(pin, led.off_value);
    }
    else
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, invert ? HIGH : LOW);
    }

    leds.push_back(led);
}

MSYS_LED_T *MSYS_LED::getLED(String name)
{
    for (int i = 0; i < leds.size(); i++)
    {
        if (leds[i].name == name)
        {
            return &leds[i];
        }
    }
    return NULL;
}

void MSYS_LED::removeLED(String name)
{
    MSYS_LED_T *led = getLED(name);
    for (int i = 0; i < leds.size(); i++)
    {
        if (leds[i].name == name)
        {
            leds.erase(leds.begin() + i);

            if (leds[i].use_pwm)
            {
                analogWrite(leds[i].pin, leds[i].off_value);
            }
            else
            {
                digitalWrite(leds[i].pin, leds[i].invert ? HIGH : LOW);
            }
            break;
        }
    }
}

void MSYS_LED::setLEDState(String name, _LED_STATE state)
{
    MSYS_LED_T *led = getLED(name);
    if (led != NULL)
    {
        if (led->state == state)
            return;
        led->current_state_index = 0;
        led->state = state;
    }
}

void MSYS_LED::threadLED(void *args)
{
    MSYS_LED *led = (MSYS_LED *)args;
    led->is_thread_running = true;
    int loop_count = 0;
    while (led->is_thread_running)
    {
        for (int i = 0; i < led->leds.size(); i++)
        {
            bool on = led->leds[i].state & (1 << (15 - led->leds[i].current_state_index));
            on = led->leds[i].invert ? !on : on;

            if (led->leds[i].use_pwm)
            {
                if (on)
                {
                    analogWrite(led->leds[i].pin, led->leds[i].on_value);
                }
                else
                {
                    analogWrite(led->leds[i].pin, led->leds[i].off_value);
                }
            }
            else
            {
                digitalWrite(led->leds[i].pin, on ? HIGH : LOW);
            }
            led->leds[i].current_state_index = (led->leds[i].current_state_index + 1) % 16;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}