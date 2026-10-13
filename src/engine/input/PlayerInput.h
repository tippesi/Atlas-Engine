#pragma once

#include "System.h"
#include "Keyboard.h"
#include "Mouse.h"

namespace Atlas {

    namespace Scene {
        class Scene;
    }

    namespace Input {

        /**
         * Controls the players of a scene with keyboard and mouse: W, A, S and D move a player relative to
         * the direction of its camera, Left Shift runs, Space jumps and the mouse rotates the camera.
         * Only players with a camera component are controlled.
         */
        class PlayerInput {

        public:
            void Update(Scene::Scene& scene, float deltaTime);

            MouseHandler mouseHandler = MouseHandler(1.5f, 8.0f);
            KeyboardHandler keyboardHandler = KeyboardHandler(7.0f, 5.0f);

            bool useMouse = true;

        };

    }

}
