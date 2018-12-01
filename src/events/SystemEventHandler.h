#ifndef SYSTEMEVENTHANDLER_H
#define SYSTEMEVENTHANDLER_H

#include "../System.h"
#include "EventChannel.h"

typedef struct SystemWindowEvent {
	uint32_t windowID;
}SystemWindowEvent;

typedef struct SystemKeyboardEvent {
	uint32_t windowID;
	bool keyDown;
	bool keyRepeat;

}SystemKeyboardEvent;

typedef struct SystemMouseButtonEvent {
	uint32_t windowID;

}SystemMouseButtonEvent;

typedef struct SystemMouseMotionEvent {
	uint32_t windowID;
	int32_t x;
	int32_t y;
	int32_t deltaX;
	int32_t deltaY;
}SystemMouseMotionEvent;

typedef struct SystemMouseWheelEvent {
	uint32_t windowID;
	int32_t x;
	int32_t y;
};

typedef struct SystemTouchEvent {
	uint32_t windowID;
}SystemTouchEvent;

typedef struct SystemMultiGestureEvent {
	uint32_t windowID;
}SystemMultiGestureEvent;

typedef struct SystemFileDropEvent {
	uint32_t windowID;
}SystemFileDropEvent;

typedef struct SystemQuitEvent {

}SystemQuitEvent;

class SystemEventHandler {

public:
	SystemEventHandler();

	void Update();

};


#endif