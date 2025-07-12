#include <Arduino.h>
#include "SoftwareLin.h"
#include "freertos/FreeRTOS.h"
#include "car_status.h"
#include "ww_bluetooth.h"
#include "HIDKeyboardTypes.h"
#include "bike_keys.h"
#include "SSD1306Wire.h"

#define LED_PIN_1 35
#define LED_PIN_2 36
#define LED_PIN_3 37

#define LIN_RX_PIN 16
#define LIN_TX_PIN 17
#define LIN_SLEEP 18

#define SWITCH_PIN_1 41
#define SWITCH_PIN_2 42

#define LIN_COMMANDER_PIN 8

SoftwareLin swLin(LIN_RX_PIN, LIN_TX_PIN);

// Change the below values if desired
#define ON_BIKE

#define BUTTON_PIN 0

#ifndef ON_BIKE
#define BUTTON_TEST_WHEEL_UP 41
#define BUTTON_TEST_WHEEL_DOWN 42
#define BUTTON_TEST_WHEEL_LEFT 48
#define BUTTON_TEST_WHEEL_RIGHT 2
#else
#define BUTTON_TEST_WHEEL_UP NOT_DEFINED_KEY
#define BUTTON_TEST_WHEEL_DOWN NOT_DEFINED_KEY
#define BUTTON_TEST_WHEEL_LEFT NOT_DEFINED_KEY
#define BUTTON_TEST_WHEEL_RIGHT NOT_DEFINED_KEY
#endif

#define BUTTON_TEST_WHEEL_LEFT_LIN (NOT_DEFINED_KEY + 1)
#define BUTTON_TEST_WHEEL_RIGHT_LIN (NOT_DEFINED_KEY + 2)
#define BUTTON_TEST_REPEATER_LEFT_LIN (NOT_DEFINED_KEY + 3)
#define BUTTON_TEST_REPEATER_RIGHT_LIN (NOT_DEFINED_KEY + 4)
#define BUTTON_TEST_REPEATER_CENTER_LIN (NOT_DEFINED_KEY + 5)

std::mutex dataMutex;
std::map<int, __BIKE_KEY *> bike_key_map;
SSD1306Wire display(0x3c, 7, 15, GEOMETRY_128_32); // I2C address 0x3c for the OLED display

int send_count = 0;
int wheel_value = 0;

void send_key_oneshot(uint8_t key);

void sendCarStatusData(void *args)
{
    // Check if the button is pressed
    String strCarStatus = "";
    dataMutex.lock(); // Lock the mutex to ensure thread safety
#ifndef ON_BIKE
    unsigned short speed = send_count++;
    speed = speed % 300;
    carStatus.setSpeed(speed);                 // Set the speed in the car status
    carStatus.setRpm(send_count * 10 % 14000); // Set the RPM in the car status
    // carStatus.setWheelValue(send_count % 256); // Simulate wheel value change for demonstration
    carStatus.setGear((send_count / 50) % 7);

    carStatus.setAllButtonRelease();

    if (digitalRead(BUTTON_TEST_WHEEL_LEFT) == LOW)
    {
        carStatus.setButtons(carStatus.BUTTON_WHEEL_LEFT);
    }
    if (digitalRead(BUTTON_TEST_WHEEL_RIGHT) == LOW)
    {
        carStatus.setButtons(carStatus.BUTTON_WHEEL_RIGHT);
    }
#endif

    if (carStatus.needToUpdate()) // Check if car status needs to be updated
    {
        wwBluetooth.CarStatusNotify(carStatus.old_car_data, CAR_DATA_LENGTH); // Notify the connected BLE clients with the current car status
    }
    dataMutex.unlock(); // Unlock the mutex after accessing shared data

    vTaskDelete(NULL); // Delete the task if it is not needed
}

void key_press_and_check_long_key(int pin)
{
    xTaskCreate(
        [](void *args)
        {
            int pin = (int)args;
            printf("key_press_and_check_long_key : %d\n", pin);
            bike_key_map[pin]->press();
            if (bike_key_map[pin]->check_long_key())
            {
                wwBluetooth.keyPress(bike_key_map[pin]->long_key);
                vTaskDelay(10);
                wwBluetooth.keyRelease(bike_key_map[pin]->long_key);
            }
            vTaskDelete(NULL);
        },
        "task_key_press_and_check_long_key",
        8192 * 2,
        (void *)pin,
        1,
        NULL);
}

void key_release(int pin)
{
    uint8_t key = bike_key_map[pin]->release();
    if (key != 0)
    {
        wwBluetooth.keyPress(key);
        vTaskDelay(10);
        wwBluetooth.keyRelease(key);
    }
}

#ifndef ON_BIKE

void task_monitor_bike_key(void *args)
{
    while (1)
    {
        for (auto &key : bike_key_map)
        {
            if (key.second->pin == NOT_DEFINED_KEY)
            {
                continue;
            }
            if (digitalRead(key.first) == LOW)
            {
                key_press_and_check_long_key(key.first);
            }
            else
            {
                key_release(key.first);
            }
        }
        vTaskDelay(10);
    }
}
#endif

void button_interrupt()
{
    send_key_oneshot(WW_KEY_ESC);
}

void drawString(int x, int y, String str, OLEDDISPLAY_COLOR color = WHITE)
{
    display.setColor(color);
    display.drawString(x, y, str);
}

void switch_test_thread(void *args)
{
    while (1)
    {
        digitalWrite(SWITCH_PIN_1, LOW);
        vTaskDelay(1000);
        digitalWrite(SWITCH_PIN_1, HIGH);
        vTaskDelay(2000);
        digitalWrite(SWITCH_PIN_2, LOW);
        vTaskDelay(1000);
        digitalWrite(SWITCH_PIN_2, HIGH);
        vTaskDelay(2000);
    }
}

void setup()
{

    set_debug_level(DEBUG_ALL);

    Serial.begin(115200);
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), button_interrupt, FALLING);

    pinMode(LED_PIN_1, OUTPUT);
    pinMode(LED_PIN_2, OUTPUT);
    pinMode(LED_PIN_3, OUTPUT);

    digitalWrite(LED_PIN_1, LOW);
    digitalWrite(LED_PIN_2, LOW);
    digitalWrite(LED_PIN_3, LOW);

    pinMode(LIN_COMMANDER_PIN, OUTPUT);
    digitalWrite(LIN_COMMANDER_PIN, HIGH);

    pinMode(LIN_SLEEP, OUTPUT);
    digitalWrite(LIN_SLEEP, HIGH);

    display.init();
    display.clear();

    display.setColor(WHITE);
    display.setFont(ArialMT_Plain_10);
    display.setTextAlignment(TEXT_ALIGN_LEFT);
    drawString(0, 0, "BMW Motorrad", WHITE);
    display.display();

#ifndef ON_BIKE
    pinMode(BUTTON_TEST_WHEEL_UP, INPUT_PULLUP);
    pinMode(BUTTON_TEST_WHEEL_DOWN, INPUT_PULLUP);
    pinMode(BUTTON_TEST_WHEEL_LEFT, INPUT_PULLUP);
    pinMode(BUTTON_TEST_WHEEL_RIGHT, INPUT_PULLUP);
#endif
    wwBluetooth.On();

#ifdef LIN_AUTOBAUD
    swLin.begin(LIN_BAUD_MAX);
#else
    swLin.begin(9600);
#endif

    bike_key_map[BUTTON_TEST_WHEEL_UP] = new __BIKE_KEY(BUTTON_TEST_WHEEL_UP, WW_KEY_UP_ARROW);
    bike_key_map[BUTTON_TEST_WHEEL_DOWN] = new __BIKE_KEY(BUTTON_TEST_WHEEL_DOWN, WW_KEY_DOWN_ARROW);
    bike_key_map[BUTTON_TEST_WHEEL_LEFT] = new __BIKE_KEY(BUTTON_TEST_WHEEL_LEFT, WW_KEY_LEFT_ARROW, WW_KEY_ESC);
    bike_key_map[BUTTON_TEST_WHEEL_RIGHT] = new __BIKE_KEY(BUTTON_TEST_WHEEL_RIGHT, WW_KEY_RIGHT_ARROW, WW_KEY_NUM_5);
    bike_key_map[BUTTON_TEST_WHEEL_LEFT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_LEFT_ARROW, WW_KEY_ESC);
    bike_key_map[BUTTON_TEST_WHEEL_RIGHT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_RIGHT_ARROW, WW_KEY_NUM_5);
    // bike_key_map[BUTTON_TEST_REPEATER_LEFT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_LEFT_ARROW, WW_KEY_ESC);
    // bike_key_map[BUTTON_TEST_REPEATER_RIGHT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_RIGHT_ARROW, WW_KEY_NUM_5);
    bike_key_map[BUTTON_TEST_REPEATER_CENTER_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_ESC, WW_KEY_ESC);

#ifndef ON_BIKE
    xTaskCreate(
        task_monitor_bike_key,
        "task_monitor_bike_key",
        8192 * 2,
        NULL,
        1,
        NULL);
#endif

    pinMode(SWITCH_PIN_1, OUTPUT);
    pinMode(SWITCH_PIN_2, OUTPUT);
    digitalWrite(SWITCH_PIN_1, HIGH);
    digitalWrite(SWITCH_PIN_2, HIGH);

    // xTaskCreate(
    //     switch_test_thread,
    //     "switch_test_thread",
    //     1024,
    //     NULL,
    //     1,
    //     NULL);
}

void print_buffer(const uint8_t *buf, int size)
{
    for (int i = 0; i < size; ++i)
    {
        Serial.printf("0x%02X ", buf[i]);
    }
    Serial.println();
}

void send_key_oneshot(uint8_t key)
{
    int key_code = key;
    xTaskCreate(
        [](void *args)
        {
            int key_code = (int)args;
            wwBluetooth.keyPress(key_code);
            vTaskDelay(10);
            wwBluetooth.keyRelease(key_code);
            vTaskDelete(NULL);
        },
        "task_send_key_oneshot",
        8192 * 2,
        (void *)key_code,
        1,
        NULL);
}
#ifndef ON_BIKE
void loop()
{
    while (1)
    {
        xTaskCreate(
            sendCarStatusData,   // Task function
            "sendCarStatusData", // Name of the task
            8192 * 2,            // Stack size in bytes
            NULL,                // Task parameters
            1,                   // Priority
            NULL);               // Task handle
        delay(10);
    }
    delay(10);
}

#else

bool is_comm_led_on = true;

void loop()
{
    while (1)
    {
        const int frame_data_bytes = 5;

        uint8_t buf[2 + frame_data_bytes]; // 2 bytes for PID and CHECKSUM. !!! The SYNC is consumed by swLin.setAutoBaud()

        if (swLin.checkBreak())
        {
            // sw_lin.checkBreak() blocks until UART ISR gives the semaphore.

            const uint32_t commonBaud[] = {110, 300, 600, 1200, 2400, 4800, 9600, 14400, 19200, 38400, 57600, 115200};
            swLin.setAutoBaud(commonBaud, sizeof(commonBaud) / sizeof(commonBaud[0]));

            const int read_timeout = 100000; // 100ms timeout
            int start_time = micros();

            int bytes_to_read = sizeof(buf) / sizeof(buf[0]);
            int bytes_read = 0;
            while (bytes_read < bytes_to_read && micros() - start_time <= read_timeout)
            {
                bytes_read += swLin.read(buf + bytes_read, bytes_to_read - bytes_read);
                delay(0); // yield for other tasks
            }
            swLin.endFrame();

            if (is_comm_led_on)
            {
                digitalWrite(LED_PIN_1, HIGH);
            }
            else
            {
                digitalWrite(LED_PIN_1, LOW);
            }
            is_comm_led_on = !is_comm_led_on;

            print_buffer(buf, bytes_read);

            // display.clear();
            // display.drawString(0, 0, StringFormat("%02x %02x %02x %02x %02x %02x %02x", buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]));
            // display.display();

            if (bytes_read < bytes_to_read)
            {
                Serial.printf("Timeout: only %d bytes is read\n", bytes_read);
                continue;
            }

            uint8_t id = buf[0];

            dataMutex.lock();
            if (id == 0x14)
            {
                carStatus.setAllButtonRelease();
                uint8_t left_buttons1 = buf[2]; // 01=ESA, 02=ABS, 04=Hazard, 08=Cruise, 10=Left Signal, 20=Right Signal, 40=Cancel Signal, 80=Horn
                uint8_t left_buttons2 = buf[3]; // 02=High Beam, 04=Cruise Reset, 08=Cruise Set, 10=Info, 20=Trip
                uint8_t wonder_wheel_in_out = buf[4];
                uint8_t wonder_wheel_value = buf[6];

                bool left = false;
                bool right = false;
                bool repeater_right = false;
                bool repeater_left = false;
                bool repeater_center = false;

                if (wonder_wheel_in_out == 0xFD) // wonder wheel out
                {

                    carStatus.setButtons(carStatus.BUTTON_WHEEL_LEFT);
                    key_press_and_check_long_key(BUTTON_TEST_WHEEL_LEFT);
                }
                else if (wonder_wheel_in_out == 0xFE) // wonder wheel in
                {
                    carStatus.setButtons(carStatus.BUTTON_WHEEL_RIGHT);
                    key_press_and_check_long_key(BUTTON_TEST_WHEEL_RIGHT);
                }
                else
                {
                    key_release(BUTTON_TEST_WHEEL_LEFT);
                    key_release(BUTTON_TEST_WHEEL_RIGHT);
                }

                if (left_buttons1 == 0x10)
                {
                    carStatus.setButtons(carStatus.BUTTON_LEFT);
                    key_press_and_check_long_key(BUTTON_TEST_REPEATER_LEFT_LIN);
                }
                else if (left_buttons1 == 0x20)
                {
                    carStatus.setButtons(carStatus.BUTTON_RIGHT);
                    key_press_and_check_long_key(BUTTON_TEST_REPEATER_RIGHT_LIN);
                }
                else if (left_buttons1 == 0x40)
                {
                    carStatus.setButtons(carStatus.BUTTON_CENTER);
                    key_press_and_check_long_key(BUTTON_TEST_REPEATER_CENTER_LIN);
                }
                else
                {
                    key_release(BUTTON_TEST_REPEATER_LEFT_LIN);
                    key_release(BUTTON_TEST_REPEATER_RIGHT_LIN);
                    key_release(BUTTON_TEST_REPEATER_CENTER_LIN);
                }

                uint8_t ret = carStatus.setWheelValue(wonder_wheel_value);

                if (ret == 1) // up
                {
                    send_key_oneshot(WW_KEY_UP_ARROW);
                }
                else if (ret == 0) // down
                {
                    send_key_oneshot(WW_KEY_DOWN_ARROW);
                }
            }
            else if (id == 0x20) // 0x20
            {
                carStatus.setIgnition(buf[2] == 0x7F ? 1 : 0);          // 0x7F means ignition is ON
                unsigned short speed = ((buf[4] & 0x0F) << 8) | buf[3]; // Speed is in bytes 3 and 4
                double d_speed = (double)speed * 0.16;
                d_speed = d_speed + 0.5;         // Round to nearest integer
                speed = (unsigned short)d_speed; // Convert back to unsigned short

                if (buf[2] == 0x3F)
                {
                    speed = 0; // If speed is 0x3F, treat it as 0 (error case)
                }
                carStatus.setSpeed(speed); // Set the speed in km/h
            }
            // else if (id == 0x2b)
            // {
            //     // print_buffer(buf, bytes_to_read);
            // }
            // else if (id == 0x2e)
            // {
            //     // print_buffer(buf, bytes_to_read);
            // }
            // 0x2e , 0x2b 별거 없음
            else if (id == 0xe9)
            {
                carStatus.setVoltage(buf[4]); // Set the voltage * 10
            }
            else if (id == 0xe2)
            {
                unsigned char gear = buf[2] & 0xF0;
                if (gear == 0x10)
                {
                    carStatus.setGear(1); // 1단
                }
                else if (gear == 0x20)
                {
                    carStatus.setGear(0); // N단
                }
                else if (gear == 0x40)
                {
                    carStatus.setGear(2); // 2단
                }
                else if (gear == 0x70)
                {
                    carStatus.setGear(3); // 3단
                }
                else if (gear == 0x80)
                {
                    carStatus.setGear(4); // 4단
                }
                else if (gear == 0xB0)
                {
                    carStatus.setGear(5); // 5단
                }
                else if (gear == 0xD0)
                {
                    carStatus.setGear(6); // 6단
                }
                else
                {
                    carStatus.setGear(9); // F
                }

                unsigned short rpm = ((buf[2] & 0x0F) << 8) | buf[1]; // RPM is in bytes 2 and 3
                rpm = rpm * 5;
                carStatus.setRpm(rpm); // Set the RPM
            }

            dataMutex.unlock();

            xTaskCreate(
                sendCarStatusData,   // Task function
                "sendCarStatusData", // Name of the task
                8192 * 2,            // Stack size in bytes
                NULL,                // Task parameters
                1,                   // Priority
                NULL);               // Task handle
        }
    }
}
#endif