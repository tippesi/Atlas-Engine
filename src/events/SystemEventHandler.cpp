#include "SystemEventHandler.h"

EventDelegate<SystemWindowEvent> SystemEventHandler::windowEventDelegate;
EventDelegate<SystemKeyboardEvent> SystemEventHandler::keyboardEventDelegate;
EventDelegate<SystemMouseButtonEvent> SystemEventHandler::mouseButtonEventDelegate;
EventDelegate<SystemMouseMotionEvent> SystemEventHandler::mouseMotionEventDelegate;
EventDelegate<SystemMouseWheelEvent> SystemEventHandler::mouseWheelEventDelegate;
EventDelegate<> SystemEventHandler::quitEventDelegate;

void SystemEventHandler::Update() {

	SDL_Event e;

	while (SDL_PollEvent(&e)) {

		if (e.type == SDL_WINDOWEVENT) {

			SystemWindowEvent event(e.window);

			windowEventDelegate.Fire(event);

		}
		else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {

			SystemKeyboardEvent event(e.key);

			keyboardEventDelegate.Fire(event);

		}
		else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {

			SystemMouseButtonEvent event(e.button);

			mouseButtonEventDelegate.Fire(event);

		}
		else if (e.type == SDL_MOUSEMOTION) {

			SystemMouseMotionEvent event(e.motion);

			mouseMotionEventDelegate.Fire(event);

		}
		else if (e.type == SDL_MOUSEWHEEL) {

			SystemMouseWheelEvent event(e.wheel);

			mouseWheelEventDelegate.Fire(event);

		}
		else if (e.type == SDL_QUIT) {

			quitEventDelegate.Fire();

		}

	}

}