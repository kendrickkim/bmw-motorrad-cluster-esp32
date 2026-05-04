#include "MSYS_BUTTON.h"

MSYS_BUTTON gMsysButton;

MSYS_BUTTON::MSYS_BUTTON()
{
    buttons.clear();
    xTaskCreate(checkButtonThread, "checkButtonThread", 8 * 1024, NULL, 1, NULL);
}

void MSYS_BUTTON::checkButtonThread(void *args)
{
    while (true)
    {
        for (int i = 0; i < gMsysButton.buttons.size(); i++)
        {
            if (digitalRead(gMsysButton.buttons[i].pin) == LOW)
            {
                if (!gMsysButton.buttons[i].isPressed)
                {
                    gMsysButton.buttons[i].isPressed = true;
                    gMsysButton.buttons[i].millis_last_press = millis();

                    if (gMsysButton.buttons[i].callback != NULL)
                    {
                        ((__BUTTON_CALLBACK)gMsysButton.buttons[i].callback)(&gMsysButton.buttons[i]);
                    }
                }
                else
                {


                }
            }
            else if (gMsysButton.buttons[i].isPressed)
            {
                gMsysButton.buttons[i].isPressed = false;
                if (gMsysButton.buttons[i].callback_release != NULL)
                {
                    ((__BUTTON_CALLBACK)gMsysButton.buttons[i].callback_release)(&gMsysButton.buttons[i]);
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void MSYS_BUTTON::addButton(int pin, __BUTTON_CALLBACK callback, __BUTTON_CALLBACK callback_release)
{
    MSYS_BUTTON_T button = {pin, false, 0, 0, (void *)callback, (void *)callback_release};
    gMsysButton.buttons.push_back(button);
}

void MSYS_BUTTON::removeButton(int pin)
{
    for (int i = 0; i < gMsysButton.buttons.size(); i++)
    {
        if (gMsysButton.buttons[i].pin == pin)
        {
            gMsysButton.buttons.erase(gMsysButton.buttons.begin() + i);
        }
    }
}
