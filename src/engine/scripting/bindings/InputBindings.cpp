#include "InputBindings.h"

#include "input/Keyboard.h"
#include "input/KeyboardMap.h"
#include "input/ControllerMap.h"

namespace Atlas::Scripting::Bindings {

    void GenerateKeyboardBindings(sol::table* ns) {

        ns->new_enum("Keycode", 
            "KeyUnknown", Keycode::KeyUnknown,
            "KeyEnter", Keycode::KeyEnter,
            "KeyEscape", Keycode::KeyEscape,
            "KeyBackspace", Keycode::KeyBackspace,
            "KeyTab", Keycode::KeyTab,
            "KeySpace", Keycode::KeySpace,
            "KeyExclaim", Keycode::KeyExclaim,
            "KeyDoubleQuote", Keycode::KeyDoubleQuote,
            "KeyHash", Keycode::KeyHash,
            "KeyPercent", Keycode::KeyPercent,
            "KeyDollar", Keycode::KeyDollar,
            "KeyAnd", Keycode::KeyAnd,
            "KeyQuote", Keycode::KeyQuote,
            "KeyLeftParen", Keycode::KeyLeftParen,
            "KeyRightParen", Keycode::KeyRightParen,
            "KeyAsterisk", Keycode::KeyAsterisk,
            "KeyPlus", Keycode::KeyPlus,
            "KeyComma", Keycode::KeyComma,
            "KeyMinus", Keycode::KeyMinus,
            "KeyPeriod", Keycode::KeyPeriod,
            "KeySlash", Keycode::KeySlash,
            "Key0", Keycode::Key0,
            "Key1", Keycode::Key1,
            "Key2", Keycode::Key2,
            "Key3", Keycode::Key3,
            "Key4", Keycode::Key4,
            "Key5", Keycode::Key5,
            "Key6", Keycode::Key6,
            "Key7", Keycode::Key7,
            "Key8", Keycode::Key8,
            "Key9", Keycode::Key9,
            "KeyColon", Keycode::KeyColon,
            "KeySemicolon", Keycode::KeySemicolon,
            "KeyLess", Keycode::KeyLess,
            "KeyEquals", Keycode::KeyEquals,
            "KeyGreater", Keycode::KeyGreater,
            "KeyQuestion", Keycode::KeyQuestion,
            "KeyAt", Keycode::KeyAt,
            "KeyLeftBracket", Keycode::KeyLeftBracket,
            "KeyBackSlash", Keycode::KeyBackSlash,
            "KeyRightBracket", Keycode::KeyRightBracket,
            "KeyCaret", Keycode::KeyCaret,
            "KeyUnderscore", Keycode::KeyUnderscore,
            "KeyBackquote", Keycode::KeyBackquote,
            "KeyA", Keycode::KeyA,
            "KeyB", Keycode::KeyB,
            "KeyC", Keycode::KeyC,
            "KeyD", Keycode::KeyD,
            "KeyE", Keycode::KeyE,
            "KeyF", Keycode::KeyF,
            "KeyG", Keycode::KeyG,
            "KeyH", Keycode::KeyH,
            "KeyI", Keycode::KeyI,
            "KeyJ", Keycode::KeyJ,
            "KeyK", Keycode::KeyK,
            "KeyL", Keycode::KeyL,
            "KeyM", Keycode::KeyM,
            "KeyN", Keycode::KeyN,
            "KeyO", Keycode::KeyO,
            "KeyP", Keycode::KeyP,
            "KeyQ", Keycode::KeyQ,
            "KeyR", Keycode::KeyR,
            "KeyS", Keycode::KeyS,
            "KeyT", Keycode::KeyT,
            "KeyU", Keycode::KeyU,
            "KeyV", Keycode::KeyV,
            "KeyW", Keycode::KeyW,
            "KeyX", Keycode::KeyX,
            "KeyY", Keycode::KeyY,
            "KeyZ", Keycode::KeyZ,
            "KeyCapsLock", Keycode::KeyCapsLock,
            "KeyF1", Keycode::KeyF1,
            "KeyF2", Keycode::KeyF2,
            "KeyF3", Keycode::KeyF3,
            "KeyF4", Keycode::KeyF4,
            "KeyF5", Keycode::KeyF5,
            "KeyF6", Keycode::KeyF6,
            "KeyF7", Keycode::KeyF7,
            "KeyF8", Keycode::KeyF8,
            "KeyF9", Keycode::KeyF9,
            "KeyF10", Keycode::KeyF10,
            "KeyF11", Keycode::KeyF11,
            "KeyF12", Keycode::KeyF12,
            "KeyPrintScreen", Keycode::KeyPrintScreen,
            "KeyScrollLock", Keycode::KeyScrollLock,
            "KeyPause", Keycode::KeyPause,
            "KeyInsert", Keycode::KeyInsert,
            "KeyHome", Keycode::KeyHome,
            "KeyPageUp", Keycode::KeyPageUp,
            "KeyDelete", Keycode::KeyDelete,
            "KeyEnd", Keycode::KeyEnd,
            "KeyPageDown", Keycode::KeyPageDown,
            "KeyRight", Keycode::KeyRight,
            "KeyLeft", Keycode::KeyLeft,
            "KeyDown", Keycode::KeyDown,
            "KeyUp", Keycode::KeyUp,
            "KeyLeftControl", Keycode::KeyLeftControl,
            "KeyLeftShift", Keycode::KeyLeftShift,
            "KeyLeftAlt", Keycode::KeyLeftAlt,
            "KeyRightControl", Keycode::KeyRightControl,
            "KeyRightShift", Keycode::KeyRightShift,
            "KeyRightAlt", Keycode::KeyRightAlt
        );

        ns->new_usertype<Input::KeyboardMap>("KeyboardMap",
            "GetKeyState", Input::KeyboardMap::GetKeyState,
            "IsKeyPressed", Input::KeyboardMap::IsKeyPressed
        );

    }

    void GenerateControllerBindings(sol::table* ns) {

        ns->new_enum("ControllerButton",
            "A", AE_CONTROLLERBUTTON_A,
            "B", AE_CONTROLLERBUTTON_B,
            "X", AE_CONTROLLERBUTTON_X,
            "Y", AE_CONTROLLERBUTTON_Y,
            "DPadUp", AE_CONTROLLERBUTTON_UP,
            "DPadDown", AE_CONTROLLERBUTTON_DOWN,
            "DPadLeft", AE_CONTROLLERBUTTON_LEFT,
            "DPadRight", AE_CONTROLLERBUTTON_RIGHT,
            "LeftStick", AE_CONTROLLERBUTTON_LEFTSTICK,
            "RightStick", AE_CONTROLLERBUTTON_RIGHTSTICK,
            "LeftShoulder", AE_CONTROLLERBUTTON_LEFTSHOULDER,
            "RightShoulder", AE_CONTROLLERBUTTON_RIGHTSHOULDER,
            "Back", AE_CONTROLLERBUTTON_BACK,
            "Guide", AE_CONTROLLERBUTTON_GUIDE,
            "Start", AE_CONTROLLERBUTTON_START
        );

        ns->new_enum("ControllerAxis",
            "LeftX", AE_CONTROLLERAXIS_LEFTX,
            "LeftY", AE_CONTROLLERAXIS_LEFTY,
            "RightX", AE_CONTROLLERAXIS_RIGHTX,
            "RightY", AE_CONTROLLERAXIS_RIGHTY,
            "LeftTrigger", AE_CONTROLLERAXIS_LEFTTRIGGER,
            "RightTrigger", AE_CONTROLLERAXIS_RIGHTTRIGGER
        );

        ns->new_usertype<Input::ControllerMap>("ControllerMap",
            "IsConnected", Input::ControllerMap::IsConnected,
            "IsButtonPressed", Input::ControllerMap::IsButtonPressed,
            "GetAxis", Input::ControllerMap::GetAxis
        );

    }

}