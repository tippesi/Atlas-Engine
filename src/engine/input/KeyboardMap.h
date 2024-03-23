#include "../System.h"

#include "../events/EventManager.h"

#include <unordered_map>

namespace Atlas::Input {

    class KeyboardMap {

    public:
        static void Init();

        static void Shutdown();

        static uint8_t GetKeyState(Keycode keyCode);

        static bool IsKeyPressed(Keycode keyCode, bool repeat = true);

    private:
        struct KeyState {
            uint8_t state;

            bool repeat;
            bool down;
        };

        static void HandleKeyboardEvent(const Events::KeyboardEvent& event);

        static std::unordered_map<Keycode, KeyState> keyMap;

    };

}