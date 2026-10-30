#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_CONTROLLERAXIS_LEFTX         SDL_GAMEPAD_AXIS_LEFTX
#define AE_CONTROLLERAXIS_LEFTY         SDL_GAMEPAD_AXIS_LEFTY
#define AE_CONTROLLERAXIS_RIGHTX         SDL_GAMEPAD_AXIS_RIGHTX
#define AE_CONTROLLERAXIS_RIGHTY         SDL_GAMEPAD_AXIS_RIGHTY
#define AE_CONTROLLERAXIS_LEFTTRIGGER     SDL_GAMEPAD_AXIS_LEFT_TRIGGER
#define AE_CONTROLLERAXIS_RIGHTTRIGGER SDL_GAMEPAD_AXIS_RIGHT_TRIGGER

namespace Atlas {

    namespace Events {

        /**
          * A class to distribute controller axis events.
         */
        class ControllerAxisEvent {

        public:
            explicit ControllerAxisEvent(SDL_GamepadAxisEvent event) {

                axis = event.axis;
                value = event.value;
                device = int32_t(event.which);

            }

            /**
             * The axis which was moved on the controller. See {@link ControllerAxisEvent.h} for more.
             */
            uint8_t axis;

            /**
             * The value of the axis. Ranges from -32767-32767.
             */
            int16_t value;

            /**
             * The device ID of the game controller.
             */
            int32_t device;

        };

    }

}