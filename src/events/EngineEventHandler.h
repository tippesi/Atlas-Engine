#ifndef ENGINEEVENTHANDLER_H
#define ENGINEEVENTHANDLER_H

#define BUTTON_RELEASED 0
#define BUTTON_PRESSED 1

#include "../System.h"

#include "EngineWindowEvent.h"
#include "EngineKeyboardEvent.h"
#include "EngineMouseButtonEvent.h"
#include "EngineMouseMotionEvent.h"
#include "EngineMouseWheelEvent.h"
#include "EngineControllerAxisEvent.h"
#include "EngineControllerButtonEvent.h"
#include "EngineControllerDeviceEvent.h"
#include "EngineTextInputEvent.h"

#include <mutex>
#include <unordered_map>

class EngineEventHandler {

public:
	static void Init();

	static void Update();

	static EventDelegate<EngineWindowEvent> WindowEventDelegate;
	static EventDelegate<EngineKeyboardEvent> KeyboardEventDelegate;
	static EventDelegate<EngineMouseButtonEvent> MouseButtonEventDelegate;
	static EventDelegate<EngineMouseMotionEvent> MouseMotionEventDelegate;
	static EventDelegate<EngineMouseWheelEvent> MouseWheelEventDelegate;
	static EventDelegate<EngineControllerAxisEvent> ControllerAxisEventDelegate;
	static EventDelegate<EngineControllerButtonEvent> ControllerButtonEventDelegate;
	static EventDelegate<EngineControllerDeviceEvent> ControllerDeviceEventDelegate;
	static EventDelegate<EngineTextInputEvent> TextInputEventDelegate;
	static EventDelegate<> QuitEventDelegate;

private:
	struct ControllerDevice {
		SDL_Joystick* joystick;
		SDL_GameController* controller;
		SDL_Haptic* haptic;
	};

	static mutex handlerMutex;
	static unordered_map<int32_t, ControllerDevice> controllers;

};


#endif