#ifndef ENGINETEXTINPUTEVENT_H
#define ENGINETEXTINPUTEVENT_H


#include "../System.h"
#include "../libraries/SDL/include/SDL.h"
#include "EventDelegate.h"
#include "EngineKeycodes.h"

/**
 * A class to distribute text input events.
 */
class EngineTextInputEvent {

public:
    EngineTextInputEvent(SDL_TextInputEvent event) {

        windowID = event.windowID;
        character = event.text[0];

    }

    /**
     * The ID of the window the event occurred in.
     */
    uint32_t windowID;

    /**
     * The character that was typed. Is still in ASCII.
     */
    char character;

};


#endif
