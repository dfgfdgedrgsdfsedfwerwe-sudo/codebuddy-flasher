#ifndef  BUTTON_HANDLER_
#define BUTTON_HANDLER_

#include "initBoard.h"

typedef void (*CBFunc)(void);

class Button
{
public:
    /**
     * @fn Button
     * @brief Constructor for a single button
     * @param _io Button pin
     */
    Button(uint8_t _io);

    /**
     * @fn Button
     * @brief Constructor for a combination of buttons
     * @param _io1 Button pin 1
     * @param _io2 Button pin 2
     */
    Button(uint8_t _io1, uint8_t _io2);

    /**
     * @fn isPressed
     * @brief Check if the button is pressed
     */
    bool isPressed(void);

    /**
     * @fn setPressedCallback
     * @brief Set callback function for button press
     * @param _cb Callback function
     */
    void setPressedCallback(CBFunc _cb);

    /**
     * @fn setUnPressedCallback
     * @brief Set callback function for button release
     * @param _cb Callback function
     */
    void setUnPressedCallback(CBFunc _cb);


private:
    CBFunc pressedCb;
    CBFunc unpressedCb;
    uint8_t io;
    uint8_t io1;
    uint8_t io2;
    uint8_t pinNum;
    bool prevState;

    static void taskLoop(void *param)
    {
        Button *self = (Button *)param;
        while (1)
        {
            bool isPressed = self->isPressed();
            if (self->prevState != isPressed)
            {
                if (isPressed && self->pressedCb)
                {
                    self->pressedCb();
                }
                if ((!isPressed) && self->unpressedCb)
                {
                    self->unpressedCb();
                }
                self->prevState = isPressed;
            }
            vTaskDelay(10);
        }
    }
};

#endif // !BUTTON_HANDLER_