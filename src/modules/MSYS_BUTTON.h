#pragma once

#include <Arduino.h>
#include <vector>

typedef struct
{
    int pin;
    bool isPressed;
    long millis_last_press;
    int flags;
    void* callback;
    void* callback_release;
} MSYS_BUTTON_T;

typedef void (*__BUTTON_CALLBACK)(MSYS_BUTTON_T* button);

class MSYS_BUTTON
{

private:
    std::vector<MSYS_BUTTON_T> buttons;

public:
    MSYS_BUTTON();
    void addButton(int pin, __BUTTON_CALLBACK callback, __BUTTON_CALLBACK callback_release = NULL);
    void removeButton(int pin);
    static void checkButtonThread(void *args);
};

extern MSYS_BUTTON gMsysButton;
