#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_WINDOWEVENT_SHOWN             SDL_EVENT_WINDOW_SHOWN
#define AE_WINDOWEVENT_HIDDEN             SDL_EVENT_WINDOW_HIDDEN
#define AE_WINDOWEVENT_EXPOSED             SDL_EVENT_WINDOW_EXPOSED
#define AE_WINDOWEVENT_MOVED             SDL_EVENT_WINDOW_MOVED
#define AE_WINDOWEVENT_RESIZED             SDL_EVENT_WINDOW_RESIZED
#define AE_WINDOWEVENT_SIZE_CHANGED     SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED
#define AE_WINDOWEVENT_MINIMIZED         SDL_EVENT_WINDOW_MINIMIZED
#define AE_WINDOWEVENT_MAXIMIZED         SDL_EVENT_WINDOW_MAXIMIZED
#define AE_WINDOWEVENT_RESTORED         SDL_EVENT_WINDOW_RESTORED
#define AE_WINDOWEVENT_MOUSE_ENTERED     SDL_EVENT_WINDOW_MOUSE_ENTER
#define AE_WINDOWEVENT_MOUSE_LEAVED     SDL_EVENT_WINDOW_MOUSE_LEAVE
#define AE_WINDOWEVENT_FOCUS_GAINED     SDL_EVENT_WINDOW_FOCUS_GAINED
#define AE_WINDOWEVENT_FOCUS_LOST         SDL_EVENT_WINDOW_FOCUS_LOST
#define AE_WINDOWEVENT_CLOSE             SDL_EVENT_WINDOW_CLOSE_REQUESTED

namespace Atlas {

    namespace Events {

        /**
         * A class to distribute window events.
         */
        class WindowEvent {

        public:
            explicit WindowEvent(SDL_WindowEvent event) :
                data(event.data1, event.data2) {
                windowID = event.windowID;
                type = event.type;
            }

            /**
             * The ID of the window the event occurred in
             */
            uint32_t windowID;

            /**
             * The type of the window event. See {@link WindowEvent.h} for more
             */
            uint32_t type;

            /**
             * The data specific to the event. Represents screen space coordinates
             */
            ivec2 data;

        };

    }

}
