#pragma once

#include "../System.h"
#include "EventDelegate.h"

#define AE_APPEVENT_TERMINATING SDL_APP_TERMINATING
#define AE_APPEVENT_LOWMEMORY SDL_APP_LOWMEMORY
#define AE_APPEVENT_WILLENTERBACKGROUND SDL_APP_WILLENTERBACKGROUND
#define AE_APPEVENT_DIDENTERBACKGROUND SDL_APP_DIDENTERBACKGROUND
#define AE_APPEVENT_WILLENTERFOREGROUND SDL_APP_WILLENTERFOREGROUND
#define AE_APPEVENT_DIDENTERFOREGROUND SDL_APP_DIDENTERFOREGROUND

namespace Atlas {

    namespace Events {

        class AppEvent {

        public:
            explicit AppEvent(SDL_Event event) : type(event.type) {}

            uint32_t type = 0;

        };

    }

}
