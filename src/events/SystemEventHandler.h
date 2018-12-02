#ifndef SYSTEMEVENTHANDLER_H
#define SYSTEMEVENTHANDLER_H

#include "../System.h"
#include "SystemWindowEvent.h"
#include "SystemKeyboardEvent.h"
#include "SystemMouseButtonEvent.h"
#include "SystemMouseMotionEvent.h"
#include "SystemMouseWheelEvent.h"

class SystemEventHandler {

public:
	static void Update();

	static EventDelegate<SystemWindowEvent> windowEventDelegate;
	static EventDelegate<SystemKeyboardEvent> keyboardEventDelegate;
	static EventDelegate<SystemMouseButtonEvent> mouseButtonEventDelegate;
	static EventDelegate<SystemMouseMotionEvent> mouseMotionEventDelegate;
	static EventDelegate<SystemMouseWheelEvent> mouseWheelEventDelegate;
	static EventDelegate<> quitEventDelegate;

};


#endif