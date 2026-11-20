#include "EventManager.h"

#include "../Clock.h"

namespace Atlas {

    namespace Events {

        EventDelegate<WindowEvent> EventManager::WindowEventDelegate;
        EventDelegate<KeyboardEvent> EventManager::KeyboardEventDelegate;
        EventDelegate<MouseButtonEvent> EventManager::MouseButtonEventDelegate;
        EventDelegate<MouseMotionEvent> EventManager::MouseMotionEventDelegate;
        EventDelegate<MouseWheelEvent> EventManager::MouseWheelEventDelegate;
        EventDelegate<ControllerAxisEvent> EventManager::ControllerAxisEventDelegate;
        EventDelegate<ControllerButtonEvent> EventManager::ControllerButtonEventDelegate;
        EventDelegate<ControllerDeviceEvent> EventManager::ControllerDeviceEventDelegate;
        EventDelegate<TouchEvent> EventManager::TouchEventDelegate;
        EventDelegate<AppEvent> EventManager::AppEventDelegate;
        EventDelegate<TextInputEvent> EventManager::TextInputEventDelegate;
        EventDelegate<AudioDeviceEvent> EventManager::AudioDeviceEventDelegate;
        EventDelegate<DropEvent> EventManager::DropEventDelegate;
        EventDelegate<FrameEvent> EventManager::FrameEventDelegate;
        EventDelegate<> EventManager::QuitEventDelegate;
        EventDelegate<> EventManager::ShutdownEventDelegate;

        std::mutex EventManager::handlerMutex;

        void EventManager::Update() {

            std::lock_guard<std::mutex> guard(handlerMutex);

            FrameEvent clockEvent(Clock::GetDelta());

            FrameEventDelegate.Fire(clockEvent);

            SDL_Event e;

            while (SDL_PollEvent(&e)) {

                if (e.type == SDL_EVENT_TERMINATING || e.type == SDL_EVENT_LOW_MEMORY ||
                    e.type == SDL_EVENT_WILL_ENTER_BACKGROUND || e.type == SDL_EVENT_DID_ENTER_BACKGROUND ||
                    e.type == SDL_EVENT_WILL_ENTER_FOREGROUND || e.type == SDL_EVENT_DID_ENTER_FOREGROUND) {

                    AppEvent event(e);

                    AppEventDelegate.Fire(event);

                } else if (e.type >= SDL_EVENT_WINDOW_FIRST && e.type <= SDL_EVENT_WINDOW_LAST) {

                    WindowEvent event(e.window);

                    WindowEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_KEY_UP) {

                    KeyboardEvent event(e.key);

                    KeyboardEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_UP) {

                    MouseButtonEvent event(e.button);

                    MouseButtonEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_MOUSE_MOTION) {

                    MouseMotionEvent event(e.motion);

                    MouseMotionEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_MOUSE_WHEEL) {

                    MouseWheelEvent event(e.wheel);

                    MouseWheelEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {

                    ControllerAxisEvent event(e.gaxis);

                    ControllerAxisEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN || e.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {

                    ControllerButtonEvent event(e.gbutton);

                    ControllerButtonEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_GAMEPAD_ADDED || e.type == SDL_EVENT_GAMEPAD_REMOVED ||
                    e.type == SDL_EVENT_GAMEPAD_REMAPPED) {

                    // Gamepads only send events while they are open
                    if (e.type == SDL_EVENT_GAMEPAD_ADDED)
                        SDL_OpenGamepad(e.gdevice.which);
                    else if (e.type == SDL_EVENT_GAMEPAD_REMOVED)
                        SDL_CloseGamepad(SDL_GetGamepadFromID(e.gdevice.which));

                    ControllerDeviceEvent event(e.gdevice);

                    ControllerDeviceEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_FINGER_MOTION || e.type == SDL_EVENT_FINGER_DOWN || e.type == SDL_EVENT_FINGER_UP ||
                    e.type == SDL_EVENT_FINGER_CANCELED) {

                    TouchEvent event(e.tfinger);

                    TouchEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_TEXT_INPUT) {

                    TextInputEvent event(e.text);

                    TextInputEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_AUDIO_DEVICE_ADDED || e.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) {

                    AudioDeviceEvent event(e.adevice);

                    AudioDeviceEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_DROP_FILE || e.type == SDL_EVENT_DROP_BEGIN ||
                    e.type == SDL_EVENT_DROP_COMPLETE || e.type == SDL_EVENT_DROP_TEXT) {

                    DropEvent event(e.drop);

                    DropEventDelegate.Fire(event);

                } else if (e.type == SDL_EVENT_QUIT) {

                    QuitEventDelegate.Fire();

                }

            }

        }

        void EventManager::EnableTextInput() {

            int32_t windowCount = 0;
            auto windows = SDL_GetWindows(&windowCount);
            for (int32_t i = 0; i < windowCount; i++)
                SDL_StartTextInput(windows[i]);
            SDL_free(windows);

        }

        void EventManager::DisableTextInput() {

            int32_t windowCount = 0;
            auto windows = SDL_GetWindows(&windowCount);
            for (int32_t i = 0; i < windowCount; i++)
                SDL_StopTextInput(windows[i]);
            SDL_free(windows);

        }

    }

}
