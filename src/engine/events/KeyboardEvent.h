#ifndef AE_KEYBOARDEVENT_H
#define AE_KEYBOARDEVENT_H

#include "../System.h"
#include "EventDelegate.h"
#include "Keycodes.h"

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute keyboard events.
         */
        class KeyboardEvent {

        public:
            explicit KeyboardEvent(SDL_KeyboardEvent event) {

                windowID = event.windowID;
                keyCode = event.keysym.sym;
                keyModifiers = event.keysym.mod;
                state = event.state;
                repeat = event.repeat > 0 ? true : false;
                down = event.type == SDL_KEYDOWN;

            }

            /**
             * The ID of the window the event occurred in.
             */
            uint32_t windowID;

            /**
             * The code of the key. See {@link Keycodes.h} for more.
             */
            Keycode keyCode;

            /**
             * The key modifiers
             */
            uint16_t keyModifiers;

            /**
             * The state of the button. Might be BUTTON_PRESSED or BUTTON_RELEASED.
             */
            uint8_t state;

            /**
             * True if the key was pressed for longer. False otherwise.
             */
            bool repeat;

            /**
             * Whether this event is triggered by the button going down or up
             */
            bool down;

        };

    }

}


#endif