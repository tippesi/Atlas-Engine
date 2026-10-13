#pragma once

#include "../System.h"
#include "../events/EventManager.h"

#include <unordered_map>

namespace Atlas::Input {

    // Combined state of all connected game controllers, the counterpart of the keyboard map
    class ControllerMap {

    public:
        static void Init();

        static void Shutdown();

        static void Update();

        static bool IsConnected();

        // Without repeat only the first update after the button went down reports it as pressed
        static bool IsButtonPressed(uint8_t button, bool repeat = true);

        // Sticks are in [-1, 1] with positive values to the right and down, triggers in [0, 1]
        static float GetAxis(uint8_t axis);

    private:
        struct ButtonState {
            bool pressed;
            bool repeat;
        };

        static void HandleButtonEvent(const Events::ControllerButtonEvent& event);

        static void HandleAxisEvent(const Events::ControllerAxisEvent& event);

        static void HandleDeviceEvent(const Events::ControllerDeviceEvent& event);

        static std::unordered_map<uint8_t, ButtonState> buttons;
        static std::unordered_map<uint8_t, float> axes;

    };

}
