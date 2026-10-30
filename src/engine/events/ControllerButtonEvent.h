#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_CONTROLLERBUTTON_A                 SDL_GAMEPAD_BUTTON_SOUTH
#define AE_CONTROLLERBUTTON_B                 SDL_GAMEPAD_BUTTON_EAST
#define AE_CONTROLLERBUTTON_X                 SDL_GAMEPAD_BUTTON_WEST
#define AE_CONTROLLERBUTTON_Y                 SDL_GAMEPAD_BUTTON_NORTH
#define AE_CONTROLLERBUTTON_UP                 SDL_GAMEPAD_BUTTON_DPAD_UP
#define AE_CONTROLLERBUTTON_DOWN             SDL_GAMEPAD_BUTTON_DPAD_DOWN
#define AE_CONTROLLERBUTTON_LEFT             SDL_GAMEPAD_BUTTON_DPAD_LEFT
#define AE_CONTROLLERBUTTON_RIGHT             SDL_GAMEPAD_BUTTON_DPAD_RIGHT
#define AE_CONTROLLERBUTTON_LEFTSTICK         SDL_GAMEPAD_BUTTON_LEFT_STICK
#define AE_CONTROLLERBUTTON_RIGHTSTICK         SDL_GAMEPAD_BUTTON_RIGHT_STICK
#define AE_CONTROLLERBUTTON_LEFTSHOULDER     SDL_GAMEPAD_BUTTON_LEFT_SHOULDER
#define AE_CONTROLLERBUTTON_RIGHTSHOULDER     SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER
#define AE_CONTROLLERBUTTON_BACK             SDL_GAMEPAD_BUTTON_BACK
#define AE_CONTROLLERBUTTON_GUIDE             SDL_GAMEPAD_BUTTON_GUIDE
#define AE_CONTROLLERBUTTON_START             SDL_GAMEPAD_BUTTON_START

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute controller button events.
         */
        class ControllerButtonEvent {

        public:
            explicit ControllerButtonEvent(SDL_GamepadButtonEvent event) {

                button = event.button;
                state = event.down ? AE_BUTTON_PRESSED : AE_BUTTON_RELEASED;
                device = int32_t(event.which);

            }

            /**
             * The button which was pressed on the controller. See {@link ControllerButtonEvent.h} for more.
             */
            uint8_t button;

            /**
             * The state of the button. Might be BUTTON_PRESSED or BUTTON_RELEASED.
             */
            uint8_t state;

            /**
             * The device ID of the game controller.
             */
            int32_t device;

        };

    }

}