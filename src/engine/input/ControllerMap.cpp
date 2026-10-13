#include "ControllerMap.h"

namespace Atlas::Input {

    std::unordered_map<uint8_t, ControllerMap::ButtonState> ControllerMap::buttons;
    std::unordered_map<uint8_t, float> ControllerMap::axes;

    void ControllerMap::Init() {

        Events::EventManager::ControllerButtonEventDelegate.Subscribe(&HandleButtonEvent);
        Events::EventManager::ControllerAxisEventDelegate.Subscribe(&HandleAxisEvent);
        Events::EventManager::ControllerDeviceEventDelegate.Subscribe(&HandleDeviceEvent);

    }

    void ControllerMap::Shutdown() {



    }

    void ControllerMap::Update() {

        for (auto& [button, state] : buttons)
            state.repeat = true;

    }

    bool ControllerMap::IsConnected() {

        for (int32_t i = 0; i < SDL_NumJoysticks(); i++)
            if (SDL_IsGameController(i))
                return true;

        return false;

    }

    bool ControllerMap::IsButtonPressed(uint8_t button, bool repeat) {

        auto it = buttons.find(button);
        if (it == buttons.end())
            return false;

        return it->second.pressed && (repeat || !it->second.repeat);

    }

    float ControllerMap::GetAxis(uint8_t axis) {

        auto it = axes.find(axis);
        return it != axes.end() ? it->second : 0.0f;

    }

    void ControllerMap::HandleButtonEvent(const Events::ControllerButtonEvent& event) {

        buttons[event.button] = ButtonState {
            .pressed = event.state == SDL_PRESSED,
            .repeat = false
        };

    }

    void ControllerMap::HandleAxisEvent(const Events::ControllerAxisEvent& event) {

        axes[event.axis] = glm::clamp(float(event.value) / 32767.0f, -1.0f, 1.0f);

    }

    void ControllerMap::HandleDeviceEvent(const Events::ControllerDeviceEvent& event) {

        // Pressed buttons and deflected sticks of a removed controller would otherwise stay
        if (event.type == AE_CONTROLLER_REMOVED && !IsConnected()) {
            buttons.clear();
            axes.clear();
        }

    }

}
