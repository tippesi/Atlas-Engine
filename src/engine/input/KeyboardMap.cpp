#include "KeyboardMap.h"

namespace Atlas::Input {

    std::unordered_map<Keycode, KeyboardMap::KeyState> KeyboardMap::keyMap;

    void KeyboardMap::Init() {

        Events::EventManager::KeyboardEventDelegate.Subscribe(&HandleKeyboardEvent);

    }

    void KeyboardMap::Shutdown() {

        

    }

    uint8_t KeyboardMap::GetKeyState(Keycode code) {

        if (keyMap.contains(code)) {
            return keyMap[code].state;
        }
        else {
            return AE_BUTTON_RELEASED;
        }

    }

    bool KeyboardMap::IsKeyPressed(Keycode code, bool repeat) {

        if (keyMap.contains(code)) {
            const auto& state = keyMap[code];
            return state.state == AE_BUTTON_PRESSED && (repeat || !state.repeat);
        }
        else {
            return false;
        }

    }

    void KeyboardMap::HandleKeyboardEvent(const Events::KeyboardEvent& event) {

        keyMap[event.keyCode] = KeyState {
                .state = event.state,
                .repeat = event.repeat,
                .down = event.down
            };

    }

}