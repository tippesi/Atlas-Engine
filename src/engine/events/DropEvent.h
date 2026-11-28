#pragma once

#include "../System.h"
#include "EventDelegate.h"

#include <string>

#define AE_FILEDROP SDL_EVENT_DROP_FILE
#define AE_TEXTDROP SDL_EVENT_DROP_TEXT
#define AE_BEGINDROP SDL_EVENT_DROP_BEGIN
#define AE_COMPLETEDROP SDL_EVENT_DROP_COMPLETE

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute drop events.
         */
        class DropEvent {

        public:
            explicit DropEvent(SDL_DropEvent event) {

                windowID = event.windowID;
                if (event.data)
                    file = std::string(event.data);
                type = event.type;

            }

            /**
             * The ID of the window the event occurred in.
             */
            uint32_t windowID;

            /**
             * The path to the file. Is empty for AE_BEGINDROP and AE_COMPLETEDROP.
             */
            std::string file;

            /**
             * The type of the drop event. See {@link DropEvent.h}
             */
            uint32_t type;

        };

    }

}