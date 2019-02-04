#ifndef AE_EVENTHANDLER_H
#define AE_EVENTHANDLER_H

#define AE_BUTTON_RELEASED 0
#define AE_BUTTON_PRESSED 1

#include "../System.h"

#include "EngineWindowEvent.h"
#include "EngineKeyboardEvent.h"
#include "EngineMouseButtonEvent.h"
#include "EngineMouseMotionEvent.h"
#include "EngineMouseWheelEvent.h"
#include "EngineControllerAxisEvent.h"
#include "EngineControllerButtonEvent.h"
#include "EngineControllerDeviceEvent.h"
#include "TouchEvent.h"
#include "EngineTextInputEvent.h"

#include <mutex>
#include <unordered_map>

class EngineEventHandler {

public:
	static void Update();

	static void EnableTextInput();

	static void DisableTextInput();

	static EventDelegate<EngineWindowEvent> WindowEventDelegate;
	static EventDelegate<EngineKeyboardEvent> KeyboardEventDelegate;
	static EventDelegate<EngineMouseButtonEvent> MouseButtonEventDelegate;
	static EventDelegate<EngineMouseMotionEvent> MouseMotionEventDelegate;
	static EventDelegate<EngineMouseWheelEvent> MouseWheelEventDelegate;
	static EventDelegate<EngineControllerAxisEvent> ControllerAxisEventDelegate;
	static EventDelegate<EngineControllerButtonEvent> ControllerButtonEventDelegate;
	static EventDelegate<EngineControllerDeviceEvent> ControllerDeviceEventDelegate;
	static EventDelegate<TouchEvent> TouchEventDelegate;
	static EventDelegate<EngineTextInputEvent> TextInputEventDelegate;
	static EventDelegate<> QuitEventDelegate;

private:
	struct ControllerDevice {
		SDL_Joystick* joystick;
		SDL_GameController* controller;
		SDL_Haptic* haptic;
	};

	static std::mutex handlerMutex;
	static std::unordered_map<int32_t, ControllerDevice> controllers;

};


#endif