#ifndef AE_EVENTHANDLER_H
#define AE_EVENTHANDLER_H

#define AE_BUTTON_RELEASED 0
#define AE_BUTTON_PRESSED 1

#include "../System.h"

#include "WindowEvent.h"
#include "KeyboardEvent.h"
#include "MouseButtonEvent.h"
#include "MouseMotionEvent.h"
#include "MouseWheelEvent.h"
#include "ControllerAxisEvent.h"
#include "ControllerButtonEvent.h"
#include "ControllerDeviceEvent.h"
#include "TouchEvent.h"
#include "TextInputEvent.h"

#include <mutex>
#include <unordered_map>

namespace Events {

	class EventHandler {

	public:
		static void Update();

		static void EnableTextInput();

		static void DisableTextInput();

		static EventDelegate<WindowEvent> WindowEventDelegate;
		static EventDelegate<KeyboardEvent> KeyboardEventDelegate;
		static EventDelegate<MouseButtonEvent> MouseButtonEventDelegate;
		static EventDelegate<MouseMotionEvent> MouseMotionEventDelegate;
		static EventDelegate<MouseWheelEvent> MouseWheelEventDelegate;
		static EventDelegate<ControllerAxisEvent> ControllerAxisEventDelegate;
		static EventDelegate<ControllerButtonEvent> ControllerButtonEventDelegate;
		static EventDelegate<ControllerDeviceEvent> ControllerDeviceEventDelegate;
		static EventDelegate<TouchEvent> TouchEventDelegate;
		static EventDelegate<TextInputEvent> TextInputEventDelegate;
		static EventDelegate<> QuitEventDelegate;

	private:
		struct ControllerDevice {
			SDL_Joystick *joystick;
			SDL_GameController *controller;
			SDL_Haptic *haptic;
		};

		static std::mutex handlerMutex;
		static std::unordered_map<int32_t, ControllerDevice> controllers;

	};

}

#endif