#ifndef AE_DROPEVENT_H
#define AE_DROPEVENT_H

#include "../System.h"
#include "EventDelegate.h"

#include <SDL/include/SDL.h>
#include <string>

#define AE_FILEDROP SDL_DROPFILE
#define AE_TEXTDROP SDL_DROPTEXT
#define AE_BEGINDROP SDL_DROPBEGIN
#define AE_COMPLETEDROP SDL_DROPCOMPLETE

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute drop events.
         */
        class DropEvent {

        public:
            DropEvent(SDL_DropEvent event) {

                windowID = event.windowID;
                if (event.file)
                    file = std::string(event.file);
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


#endif