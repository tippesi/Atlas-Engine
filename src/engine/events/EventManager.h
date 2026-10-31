#pragma once

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
#include "AppEvent.h"
#include "TextInputEvent.h"
#include "AudioDeviceEvent.h"
#include "DropEvent.h"
#include "FrameEvent.h"

#include <mutex>

namespace Atlas {

    namespace Events {

        class EventManager {

        public:
            /**
             * Updates the event manager. Is called by the engine.
             */
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
            static EventDelegate<AppEvent> AppEventDelegate;
            static EventDelegate<TextInputEvent> TextInputEventDelegate;
            static EventDelegate<AudioDeviceEvent> AudioDeviceEventDelegate;
            static EventDelegate<DropEvent> DropEventDelegate;
            static EventDelegate<FrameEvent> FrameEventDelegate;
            static EventDelegate<> QuitEventDelegate; 
            static EventDelegate<> ShutdownEventDelegate;

        private:
            static std::mutex handlerMutex;

        };

    }

}
