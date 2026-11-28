#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_AUDIODEVICE_ADDED SDL_EVENT_AUDIO_DEVICE_ADDED
#define AE_AUDIODEVICE_REMOVED SDL_EVENT_AUDIO_DEVICE_REMOVED

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute audio device events.
         */
        class AudioDeviceEvent {

        public:
            explicit AudioDeviceEvent(SDL_AudioDeviceEvent event) {

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