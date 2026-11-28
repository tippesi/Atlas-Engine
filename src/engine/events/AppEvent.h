#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_APPEVENT_TERMINATING SDL_EVENT_TERMINATING
#define AE_APPEVENT_LOWMEMORY SDL_EVENT_LOW_MEMORY
#define AE_APPEVENT_WILLENTERBACKGROUND SDL_EVENT_WILL_ENTER_BACKGROUND
#define AE_APPEVENT_DIDENTERBACKGROUND SDL_EVENT_DID_ENTER_BACKGROUND
#define AE_APPEVENT_WILLENTERFOREGROUND SDL_EVENT_WILL_ENTER_FOREGROUND
#define AE_APPEVENT_DIDENTERFOREGROUND SDL_EVENT_DID_ENTER_FOREGROUND

namespace Atlas {

    namespace Events {

        class AppEvent {

        public:
            explicit AppEvent(SDL_Event event) : type(event.type) {}

            uint32_t type = 0;

        };

    }

}
