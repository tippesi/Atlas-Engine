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
		EventDelegate<TextInputEvent> EventManager::TextInputEventDelegate;
		EventDelegate<AudioDeviceEvent> EventManager::AudioDeviceEventDelegate;
		EventDelegate<DropEvent> EventManager::DropEventDelegate;
		EventDelegate<ClockEvent> EventManager::ClockEventDelegate;
		EventDelegate<> EventManager::QuitEventDelegate;

		std::mutex EventManager::handlerMutex;
		std::unordered_map<int32_t, EventManager::ControllerDevice> EventManager::controllers;

		void EventManager::Update() {

			std::lock_guard<std::mutex> guard(handlerMutex);

			ClockEvent event(Clock::GetDelta());

			ClockEventDelegate.Fire(event);

			SDL_Event e;

			while (SDL_PollEvent(&e)) {

				if (e.type == SDL_WINDOWEVENT) {

					WindowEvent event(e.window);

					WindowEventDelegate.Fire(event);

				} else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {

					KeyboardEvent event(e.key);

					KeyboardEventDelegate.Fire(event);

				} else if (e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP) {

					MouseButtonEvent event(e.button);

					MouseButtonEventDelegate.Fire(event);

				} else if (e.type == SDL_MOUSEMOTION) {

					MouseMotionEvent event(e.motion);

					MouseMotionEventDelegate.Fire(event);

				} else if (e.type == SDL_MOUSEWHEEL) {

					MouseWheelEvent event(e.wheel);

					MouseWheelEventDelegate.Fire(event);

				} else if (e.type == SDL_CONTROLLERAXISMOTION) {

					ControllerAxisEvent event(e.caxis);

					ControllerAxisEventDelegate.Fire(event);

				} else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP) {

					ControllerButtonEvent event(e.cbutton);

					ControllerButtonEventDelegate.Fire(event);

				} else if (e.type == SDL_CONTROLLERDEVICEADDED || e.type == SDL_CONTROLLERDEVICEREMOVED ||
					e.type == SDL_CONTROLLERDEVICEREMAPPED) {

					if (e.type == SDL_CONTROLLERDEVICEADDED) {
						ControllerDevice device;
						device.joystick = SDL_JoystickOpen(e.cdevice.which);
						device.controller = SDL_GameControllerOpen(e.cdevice.which);
						device.haptic = SDL_HapticOpenFromJoystick(device.joystick);
						SDL_HapticRumbleInit(device.haptic);
						e.cdevice.which = (int32_t) SDL_JoystickInstanceID(device.joystick);
						controllers[e.cdevice.which] = device;
					} else if (e.type == SDL_CONTROLLERDEVICEREMOVED) {
						auto device = controllers[e.cdevice.which];
						SDL_HapticClose(device.haptic);
						SDL_GameControllerClose(device.controller);
						SDL_JoystickClose(device.joystick);
						controllers.erase(e.cdevice.which);
					}

					ControllerDeviceEvent event(e.cdevice);

					ControllerDeviceEventDelegate.Fire(event);

				} else if (e.type == SDL_FINGERMOTION || e.type == SDL_FINGERDOWN || e.type == SDL_FINGERUP) {

					TouchEvent event(e.tfinger);

					TouchEventDelegate.Fire(event);

				} else if (e.type == SDL_TEXTINPUT) {

					TextInputEvent event(e.text);

					TextInputEventDelegate.Fire(event);

				} else if (e.type == SDL_AUDIODEVICEADDED || e.type == SDL_AUDIODEVICEREMOVED) {

					AudioDeviceEvent event(e.adevice);

					AudioDeviceEventDelegate.Fire(event);

				} else if (e.type == SDL_DROPFILE || e.type == SDL_DROPBEGIN ||
					e.type == SDL_DROPCOMPLETE || e.type == SDL_DROPTEXT) {

					DropEvent event(e.drop);

					DropEventDelegate.Fire(event);

				} else if (e.type == SDL_QUIT) {

					QuitEventDelegate.Fire();

				}

			}

		}

		void EventManager::EnableTextInput() {

			SDL_StartTextInput();

		}

		void EventManager::DisableTextInput() {

			SDL_StopTextInput();

		}

	}

}