#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_CONTROLLER_ADDED SDL_CONTROLLERDEVICEADDED
#define AE_CONTROLLER_REMOVED SDL_CONTROLLERDEVICEREMOVED
#define AE_CONTROLLER_MAPPED SDL_CONTROLLERDEVICEREMAPPED

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute controller device events.
         */
        class ControllerDeviceEvent {

        public:
            explicit ControllerDeviceEvent(SDL_ControllerDeviceEvent event) {

                type = event.type;
                device = event.which;

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