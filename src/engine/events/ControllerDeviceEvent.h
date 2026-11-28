#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_CONTROLLER_ADDED SDL_EVENT_GAMEPAD_ADDED
#define AE_CONTROLLER_REMOVED SDL_EVENT_GAMEPAD_REMOVED
#define AE_CONTROLLER_MAPPED SDL_EVENT_GAMEPAD_REMAPPED

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute controller device events.
         */
        class ControllerDeviceEvent {

        public:
            explicit ControllerDeviceEvent(SDL_GamepadDeviceEvent event) {

                type = event.type;
                device = int32_t(event.which);

            }

            /**
             * The type of the event. See {@link ControllerDeviceEvent.h} for more.
             */
            uint32_t type;

            /**
             * The device ID of the game controller.
             */
            int32_t device;

        };

    }

}