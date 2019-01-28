#ifndef ENGINETEXTINPUTEVENT_H
#define ENGINETEXTINPUTEVENT_H


#include "../System.h"
#include "EventDelegate.h"
#include "EngineKeycodes.h"

#include <SDL/include/SDL.h>

/**
 * A class to distribute text input events.
 */
class EngineTextInputEvent {

public:
    EngineTextInputEvent(SDL_TextInputEvent event) {

        windowID = event.windowID;

		char* text = event.text;

		while (*text != '\0') {
			character.push_back(*text);
			text++;
		}

    }

    /**
     * The ID of the window the event occurred in.
     */
    uint32_t windowID;

    /**
     * The character that was typed in UTF-8.
     */
    string character;

};


#endif