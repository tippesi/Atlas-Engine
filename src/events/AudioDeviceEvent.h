#ifndef AE_AUDIODEVICEEVENT_H
#define AE_AUDIODEVICEEVENT_H

#include "../System.h"
#include "EventDelegate.h"

#include <SDL/include/SDL.h>

#define AE_AUDIODEVICE_ADDED SDL_AUDIODEVICEADDED
#define AE_AUDIODEVICE_REMOVED SDL_AUDIODEVICEREMOVED

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute audio device events.
         */
        class AudioDeviceEvent {

        public:
            AudioDeviceEvent(SDL_AudioDeviceEvent event) {

                type = event.type;
                device = event.which;

            }

            /**
             * The type of the event. See {@link AudioDeviceEvent.h} for more.
             */
            uint32_t type;

            /**
             * The device ID of the audio device.
             */
            uint32_t device;

        };

    }

}

#endif
