#ifndef AE_TOUCHEVENT_H
#define AE_TOUCHEVENT_H

#include "../System.h"
#include "EventDelegate.h"
#include "../../dependencies/SDL/include/SDL_events.h"

#include <SDL/include/SDL.h>

#define AE_FINGERMOTION SDL_FINGERMOTION
#define AE_FINGERDOWN   SDL_FINGERDOWN
#define AE_FINGERUP     SDL_FINGERUP

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute touchscreen events.
         */
        class TouchEvent {

        public:
            TouchEvent(SDL_TouchFingerEvent event) {

                finger = event.fingerId;

                type = event.type;

                x = event.x;
                y = event.y;

                dx = event.dx;
                dy = event.dy;

                pressure = event.pressure;

            }

            /**
             * The ID of the finger
             */
            int64_t finger;

            /**
             * The type of the touch event. See {@link TouchEvent.h} for more.
             */
            uint32_t type;

            /**
             * The x-axis location of the finger, normalized (0, 1)
             */
            float x;

            /**
             * The y-axis location of the finger, normalized (0, 1)
             */
            float y;

            /**
             * The distance moved on the x-axis, normalized (-1, 1)
             */
            float dx;

            /**
             * The distance moved on the y-axis, normalized (-1, 1)
             */
            float dy;

            /**
             * The pressure applied by the finger, normalized (0, 1)
             */
            float pressure;

        };

    }

}

#endif