#include "Button_handler.h"



Button::Button(uint8_t _io)
    : pressedCb(NULL), unpressedCb(NULL), io(_io), io1(-1), io2(-1), pinNum(1), prevState(false)
{
}

Button::Button(uint8_t _io1, uint8_t _io2)
    : pressedCb(NULL), unpressedCb(NULL), io(-1), io1(_io1), io2(_io2), pinNum(2)
{
}

bool Button::isPressed(void)
{
    int retry = 5;
    if (pinNum == 1)
    {
        if (digital_read((ePin_t)io) == 0)
        {
            while (retry--)
            {
                if (digital_read((ePin_t)io))
                    return false;
                delay(5);
            }
            if (digital_read((ePin_t)io) == 0)
                return true;
        }
        return false;
    }
    else
    {
        if ((digital_read((ePin_t)io1) == 0) && (digital_read((ePin_t)io2) == 0))
        {
            while (retry--)
            {
                if (digital_read((ePin_t)io1) || digital_read((ePin_t)io2))
                    return false;
                delay(5);
            }
            if ((digital_read((ePin_t)io1) == 0) && (digital_read((ePin_t)io2) == 0))
                return true;
        }
        return false;
    }
}

void Button::setPressedCallback(CBFunc cb)
{
    static char name[] = {"buttonX"};
    if (pinNum == 1)
        name[6] = io + '0';
    else
        name[6] = io1 + io2 + '0';
    if (pressedCb == NULL && unpressedCb == NULL)
    {
        xTaskCreatePinnedToCore(Button::taskLoop, name, 8* 1024, this, 5, NULL,1);
    }
    pressedCb = cb;
}

void Button::setUnPressedCallback(CBFunc cb)
{
    static char name[] = {"buttonX"};
    if (pinNum == 1)
        name[6] = io + '0';
    else
        name[6] = io1 + io2 + '0';
    if (unpressedCb == NULL && pressedCb == NULL)
    {
        xTaskCreatePinnedToCore(Button::taskLoop, name,2* 1024, this, 5, NULL,1);
    }
    unpressedCb = cb;
}
