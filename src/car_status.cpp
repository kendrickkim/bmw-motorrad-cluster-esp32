#include "car_status.h"
#include <Arduino.h>
#include "common.h"

__CAR_STATUS carStatus;

__CAR_STATUS::__CAR_STATUS()
{
    buttons = 0;
    wheel_value = 0;
    gear = 0;
    voltage = 0;
    wheel_rpm = 0;
    rpm = 0;
    ignition = 0;

    // Initialize car_data and old_car_data to zero
    memset(car_data, 0, sizeof(car_data));
    memset(old_car_data, 0, sizeof(old_car_data));
}
void __CAR_STATUS::setAllButtonRelease()
{
    // Reset all button states
    buttons = 0;
}

void __CAR_STATUS::setButtons(unsigned short buttons)
{
    this->buttons |= buttons;
}

bool __CAR_STATUS::needToUpdate()
{
    buildData(); // Build the current car data
    if (memcmp(car_data, old_car_data, sizeof(car_data)) != 0)
    {
        memcpy(old_car_data, car_data, sizeof(car_data));
        return true;
    }
    return false;
}

void __CAR_STATUS::buildData()
{
    if(ignition == 0)
    {
        wheel_rpm = 0;
        rpm = 0;
    }
    car_data[0] = buttons & 0xFF;        // Low byte of buttons
    car_data[1] = (buttons >> 8) & 0xFF; // High byte of buttons
    car_data[2] = wheel_value;           // Wheel value
    car_data[3] = gear;                  // Gear
    car_data[4] = voltage;               // Voltage
    car_data[5] = wheel_rpm & 0xFF;      // Low byte of wheel RPM
    car_data[6] = (wheel_rpm >> 8) & 0xFF; // High byte of wheel RPM
    car_data[7] = rpm & 0xFF;            // Low byte of RPM
    car_data[8] = (rpm >> 8) & 0xFF;     // High byte of RPM
    car_data[9] = ignition;              // Ignition state

    // Fill the rest with zeros or any other data as needed
    memset(car_data + 11, 0, sizeof(car_data) - 11);
}

void __CAR_STATUS::printData()
{
    String str_data =
        StringFormat("LEFT:%s "
                     "RIGHT:%s "
                     "CENTER:%s "
                     "WHEEL_LEFT:%s "
                     "WHEEL_RIGHT:%s "
                     "WHEEL_VALUE:%d "
                     "GEAR:%d "
                     "VOLTAGE:%.2fV "
                     "WHEEL_RPM:%d "
                     "RPM:%d "
                     "IGNITION:%s",
                     button_to_string(buttons & BUTTON_LEFT).c_str(),
                     button_to_string(buttons & BUTTON_RIGHT).c_str(),
                     button_to_string(buttons & BUTTON_CENTER).c_str(),
                     button_to_string(buttons & BUTTON_WHEEL_LEFT).c_str(),
                     button_to_string(buttons & BUTTON_WHEEL_RIGHT).c_str(),
                     wheel_value,
                     gear,
                     voltage / 100.0,
                     wheel_rpm,
                     rpm,
                     button_to_string(ignition).c_str());

    printf("%s\n", str_data.c_str());
}

uint8_t __CAR_STATUS::setWheelValue(uint8_t value) // 2 : nothing, 0 : up : 1 down
{
    if (!wheel_value_init)
    {
        wheel_value = value;
        wheel_value_init = true;
        return 2;
    }

    uint8_t ret = 2;

    if (wheel_value > value)
    {
        ret = 0;
        if (wheel_value - value > 200)
        {
            ret = 1;
        }
    }
    else
    {
        ret = 1;
        if (value - wheel_value > 200)
        {
            ret = 0;
        }
    }

    wheel_value = value;
    return ret;
}