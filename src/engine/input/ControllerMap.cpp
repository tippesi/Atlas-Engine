#include "ControllerMap.h"
#include "common/MathHelper.h"

namespace Atlas::Input {

    std::unordered_map<uint8_t, ControllerMap::ButtonState> ControllerMap::buttons;
    std::unordered_map<uint8_t, float> ControllerMap::axes;
    bool ControllerMap::connected = false;
    bool ControllerMap::connectionChanged = true;

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

        // SDL_HasGamepad enumerates the joysticks under a lock, scripts ask many times per frame
        if (connectionChanged) {
            connected = SDL_HasGamepad();
            connectionChanged = false;
        }
        return connected;

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

    void ControllerMap::Rumble(float low, float high, float seconds) {

        if (!IsConnected())
            return;

        auto strength = [](float value) { return Uint16(Helper::Math::Saturate(value) * 65535.0f); };
        auto milliseconds = Uint32(glm::max(seconds, 0.0f) * 1000.0f);

        int32_t count = 0;
        auto ids = SDL_GetGamepads(&count);
        for (int32_t i = 0; ids && i < count; i++) {
            if (auto gamepad = SDL_GetGamepadFromID(ids[i]))
                SDL_RumbleGamepad(gamepad, strength(low), strength(high), milliseconds);
        }
        SDL_free(ids);

    }

    void ControllerMap::HandleButtonEvent(const Events::ControllerButtonEvent& event) {

        buttons[event.button] = ButtonState {
            .pressed = event.state == AE_BUTTON_PRESSED,
            .repeat = false
        };

    }

    void ControllerMap::HandleAxisEvent(const Events::ControllerAxisEvent& event) {

        axes[event.axis] = glm::clamp(float(event.value) / 32767.0f, -1.0f, 1.0f);

    }

    void ControllerMap::HandleDeviceEvent(const Events::ControllerDeviceEvent& event) {

        connectionChanged = true;
        // Pressed buttons and deflected sticks of a removed controller would otherwise stay
        if (event.type == AE_CONTROLLER_REMOVED && !IsConnected()) {
            buttons.clear();
            axes.clear();
        }

    }

}
