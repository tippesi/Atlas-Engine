#include "EngineEventHandler.h"

EventDelegate<EngineWindowEvent> EngineEventHandler::WindowEventDelegate;
EventDelegate<EngineKeyboardEvent> EngineEventHandler::KeyboardEventDelegate;
EventDelegate<EngineMouseButtonEvent> EngineEventHandler::MouseButtonEventDelegate;
EventDelegate<EngineMouseMotionEvent> EngineEventHandler::MouseMotionEventDelegate;
EventDelegate<EngineMouseWheelEvent> EngineEventHandler::MouseWheelEventDelegate;
EventDelegate<EngineControllerAxisEvent> EngineEventHandler::ControllerAxisEventDelegate;
EventDelegate<EngineControllerButtonEvent> EngineEventHandler::ControllerButtonEventDelegate;
EventDelegate<EngineControllerDeviceEvent> EngineEventHandler::ControllerDeviceEventDelegate;
EventDelegate<EngineTextInputEvent> EngineEventHandler::TextInputEventDelegate;
EventDelegate<> EngineEventHandler::QuitEventDelegate;

mutex EngineEventHandler::handlerMutex;
unordered_map<int32_t, EngineEventHandler::ControllerDevice> EngineEventHandler::controllers;

void EngineEventHandler::Update() {

	lock_guard<mutex> guard(handlerMutex);

	SDL_Event e;

	while (SDL_PollEvent(&e)) {

		if (e.type == SDL_WINDOWEVENT) {

			EngineWindowEvent event(e.window);

			WindowEventDelegate.Fire(event);

		}
		else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {

			EngineKeyboardEvent event(e.key);

			KeyboardEventDelegate.Fire(event);

		}
		else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {

			EngineMouseButtonEvent event(e.button);

			MouseButtonEventDelegate.Fire(event);

		}
		else if (e.type == SDL_MOUSEMOTION) {

			EngineMouseMotionEvent event(e.motion);

			MouseMotionEventDelegate.Fire(event);

		}
		else if (e.type == SDL_MOUSEWHEEL) {

			EngineMouseWheelEvent event(e.wheel);

			MouseWheelEventDelegate.Fire(event);

		}
		else if (e.type == SDL_CONTROLLERAXISMOTION) {

			EngineControllerAxisEvent event(e.caxis);

			ControllerAxisEventDelegate.Fire(event);

		}
		else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {

			EngineControllerButtonEvent event(e.cbutton);

			ControllerButtonEventDelegate.Fire(event);

		}
		else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED || e.type == SDL_CONTROLLERDEVICEREMAPPED) {

			if (e.type == SDL_CONTROLLERDEVICEADDED) {
				ControllerDevice device;
				device.joystick = SDL_JoystickOpen(e.cdevice.which);
				device.controller = SDL_GameControllerOpen(e.cdevice.which);
				device.haptic = SDL_HapticOpenFromJoystick(device.joystick);
				SDL_HapticRumbleInit(device.haptic);
				e.cdevice.which = (int32_t)SDL_JoystickInstanceID(device.joystick);
				controllers[e.cdevice.which] = device;
			}
			else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
				auto device = controllers[e.cdevice.which];
				SDL_HapticClose(device.haptic);
				SDL_GameControllerClose(device.controller);
				SDL_JoystickClose(device.joystick);
				controllers.erase(e.cdevice.which);
			}

			EngineControllerDeviceEvent event(e.cdevice);

			ControllerDeviceEventDelegate.Fire(event);

		}
		else if (e.type == SDL_TEXTINPUT) {

			EngineTextInputEvent event(e.text);

			TextInputEventDelegate.Fire(event);

		}
		else if (e.type == SDL_QUIT) {

			QuitEventDelegate.Fire();

		}

	}

}

void EngineEventHandler::EnableTextInput(){

	SDL_StartTextInput();

}

void EngineEventHandler::DisableTextInput(){

	SDL_StopTextInput();

}