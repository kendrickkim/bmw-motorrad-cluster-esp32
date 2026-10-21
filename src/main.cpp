#include <Arduino.h>
#include "SoftwareLin.h"
#include "freertos/FreeRTOS.h"
#include "car_status.h"
#include "ww_bluetooth.h"
#include "HIDKeyboardTypes.h"
#include "bike_keys.h"
#include "SSD1306Wire.h"

// #define LED_PIN_1 35
// #define LED_PIN_2 36
// #define LED_PIN_3 37

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
#define CAPTURE_LED_PIN 1

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

// index 0 = Message ID, index 1..6 = Data-0 .. Data-5. Checksum is not forwarded.
#define LIN_FRAME_LEN 7

static uint8_t last_lin_frame[256][LIN_FRAME_LEN];
static uint8_t lin_frame_seen[256];

void send_key_oneshot(uint8_t key);

void notify_lin_frame(const uint8_t *frame)
{
    uint8_t id = frame[0];
    if (lin_frame_seen[id] && memcmp(last_lin_frame[id], frame, LIN_FRAME_LEN) == 0)
    {
        return;
    }
    memcpy(last_lin_frame[id], frame, LIN_FRAME_LEN);
    lin_frame_seen[id] = 1;
    wwBluetooth.CarStatusNotify(last_lin_frame[id], LIN_FRAME_LEN);
}

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
    auto it = bike_key_map.find(pin);
    if (it == bike_key_map.end() || it->second == nullptr)
    {
        return;
    }

    xTaskCreate(
        [](void *args)
        {
            int pin = (int)args;
            auto it = bike_key_map.find(pin);
            if (it == bike_key_map.end() || it->second == nullptr)
            {
                vTaskDelete(NULL);
                return;
            }
            printf("key_press_and_check_long_key : %d\n", pin);
            it->second->press();
            if (it->second->check_long_key())
            {
                wwBluetooth.keyPress(it->second->long_key);
                vTaskDelay(10);
                wwBluetooth.keyRelease(it->second->long_key);
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
    auto it = bike_key_map.find(pin);
    if (it == bike_key_map.end() || it->second == nullptr)
    {
        return;
    }

    uint8_t key = it->second->release();
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

extern const uint8_t lin_capture_txt_start[] asm("_binary_lin_capture_txt_start");
extern const uint8_t lin_capture_txt_end[] asm("_binary_lin_capture_txt_end");

#define LIN_CAPTURE_HOLD_MS 5000

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

static bool next_lin_capture_frame(const uint8_t *&cursor, const uint8_t *end, uint8_t out[LIN_FRAME_LEN])
{
    while (cursor < end)
    {
        while (cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n'))
            cursor++;
        if (cursor >= end)
            return false;

        const uint8_t *line_end = cursor;
        while (line_end < end && *line_end != '\n' && *line_end != '\r')
            line_end++;

        const uint8_t *token = cursor;
        cursor = line_end;
        int count = 0;
        while (token < line_end && count < LIN_FRAME_LEN)
        {
            while (token < line_end && (*token == ' ' || *token == '\t'))
                token++;
            if (token >= line_end)
                break;
            if ((token + 1) < line_end && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
                token += 2;

            int value = 0;
            int digits = 0;
            int nibble;
            while (token < line_end && (nibble = hex_nibble((char)*token)) >= 0)
            {
                value = (value << 4) | nibble;
                digits++;
                token++;
            }
            if (digits == 0)
                break;
            out[count++] = (uint8_t)value;
        }
        if (count == LIN_FRAME_LEN)
            return true;
    }
    return false;
}

static void wait_lin_capture_button_release()
{
    while (digitalRead(BUTTON_PIN) == LOW)
        vTaskDelay(pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void replay_lin_capture()
{
    wait_lin_capture_button_release();
    const uint8_t *cursor = lin_capture_txt_start;
    const uint8_t *end = lin_capture_txt_end;
    int line = 0;
    Serial.println("LIN capture replay start");
    while (cursor < end)
    {
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            digitalWrite(CAPTURE_LED_PIN, LOW);
            Serial.println("LIN capture replay stopped");
            return;
        }
        uint8_t frame[LIN_FRAME_LEN];
        if (!next_lin_capture_frame(cursor, end, frame))
            break;
        line++;
        Serial.printf(
            "LIN sim %d: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
            line,
            frame[0], frame[1], frame[2], frame[3], frame[4], frame[5], frame[6]);
        wwBluetooth.CarStatusNotify(frame, LIN_FRAME_LEN);
        digitalWrite(CAPTURE_LED_PIN, HIGH);
        vTaskDelay(pdMS_TO_TICKS(5));
        digitalWrite(CAPTURE_LED_PIN, LOW);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
    digitalWrite(CAPTURE_LED_PIN, LOW);
    Serial.printf("LIN capture replay done (%d)\n", line);
}

static void task_lin_capture(void *)
{
    bool holding = false;
    uint32_t press_start = 0;
    while (1)
    {
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            if (!holding)
            {
                holding = true;
                press_start = millis();
            }
            else if (millis() - press_start >= LIN_CAPTURE_HOLD_MS)
            {
                replay_lin_capture();
                holding = false;
            }
        }
        else
        {
            holding = false;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
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
    pinMode(CAPTURE_LED_PIN, OUTPUT);
    digitalWrite(CAPTURE_LED_PIN, LOW);

    // pinMode(LED_PIN_1, OUTPUT);
    // pinMode(LED_PIN_2, OUTPUT);
    // pinMode(LED_PIN_3, OUTPUT);

    // digitalWrite(LED_PIN_1, LOW);
    // digitalWrite(LED_PIN_2, LOW);
    // digitalWrite(LED_PIN_3, LOW);

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

    xTaskCreate(
        task_lin_capture,
        "task_lin_capture",
        8192,
        NULL,
        1,
        NULL);
#define LIN_AUTOBAUD 1
#ifdef LIN_AUTOBAUD
    swLin.begin(20000);
#else
    swLin.begin(9600);
#endif

#ifndef ON_BIKE
    bike_key_map[BUTTON_TEST_WHEEL_UP] = new __BIKE_KEY(BUTTON_TEST_WHEEL_UP, WW_KEY_UP_ARROW);
    bike_key_map[BUTTON_TEST_WHEEL_DOWN] = new __BIKE_KEY(BUTTON_TEST_WHEEL_DOWN, WW_KEY_DOWN_ARROW);
    bike_key_map[BUTTON_TEST_WHEEL_LEFT] = new __BIKE_KEY(BUTTON_TEST_WHEEL_LEFT, WW_KEY_LEFT_ARROW, WW_KEY_ESC);
    bike_key_map[BUTTON_TEST_WHEEL_RIGHT] = new __BIKE_KEY(BUTTON_TEST_WHEEL_RIGHT, WW_KEY_RIGHT_ARROW, WW_KEY_NUM_5);
#endif
    bike_key_map[BUTTON_TEST_WHEEL_LEFT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_LEFT_ARROW, WW_KEY_ESC);
    bike_key_map[BUTTON_TEST_WHEEL_RIGHT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_RIGHT_ARROW, WW_KEY_NUM_5);
    bike_key_map[BUTTON_TEST_REPEATER_LEFT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_LEFT_ARROW, WW_KEY_ESC);
    bike_key_map[BUTTON_TEST_REPEATER_RIGHT_LIN] = new __BIKE_KEY(NOT_DEFINED_KEY, WW_KEY_RIGHT_ARROW, WW_KEY_NUM_5);
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
        uint8_t buf[LIN_FRAME_LEN]; // Message ID + Data-0..Data-5. SYNC is consumed by swLin.setAutoBaud().

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

            // if (is_comm_led_on)
            // {
            //     digitalWrite(LED_PIN_1, HIGH);
            // }
            // else
            // {
            //     digitalWrite(LED_PIN_1, LOW);
            // }
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
                // Data-1 / Data-3 / Data-5. See linbus lin_analysis ID 0x14.
                carStatus.setAllButtonRelease();
                uint8_t left_buttons1 = buf[2];
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
                    key_press_and_check_long_key(BUTTON_TEST_WHEEL_LEFT_LIN);
                }
                else if (wonder_wheel_in_out == 0xFE) // wonder wheel in
                {
                    carStatus.setButtons(carStatus.BUTTON_WHEEL_RIGHT);
                    key_press_and_check_long_key(BUTTON_TEST_WHEEL_RIGHT_LIN);
                }
                else
                {
                    key_release(BUTTON_TEST_WHEEL_LEFT_LIN);
                    key_release(BUTTON_TEST_WHEEL_RIGHT_LIN);
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

            dataMutex.unlock();

            notify_lin_frame(buf);
        }
    }
}
#endif