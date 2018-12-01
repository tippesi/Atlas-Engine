#include "SystemEventHandler.h"
#include "../libraries/SDL/include/SDL.h"

SystemEventHandler::SystemEventHandler() {
	
}

void SystemEventHandler::Update() {

	SDL_Event e;

	while (SDL_PollEvent(&e)) {

		if (e.type == SDL_WINDOWEVENT) {

			SystemWindowEvent event;
			event.windowID = e.window.windowID;

			EventChannel<SystemWindowEvent>::Publish(&event);

		}
		else if (e.type == SDL_KEYDOWN) {

		}
		else if (e.type == SDL_KEYUP) {

		}
		else if (e.type == SDL_QUIT) {

			SystemQuitEvent event;
			EventChannel<SystemQuitEvent>::Publish(event);

		}

	}

}