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
         * Game controllers move with the left stick (fully deflected or with a stick click to run), look
         * with the right stick and jump with A.
         * Only players with a camera component are controlled.
         */
        class PlayerInput {

        public:
            void Update(Scene::Scene& scene, float deltaTime);

            MouseHandler mouseHandler = MouseHandler(1.5f, 8.0f);
            KeyboardHandler keyboardHandler = KeyboardHandler(7.0f, 5.0f);

            bool useMouse = true;
            bool useController = true;

            // Radians per second at full deflection of the right stick
            float controllerLookSpeed = 2.5f;
            float controllerDeadZone = 0.15f;

            // With the right stick at rest for controllerFollowDelay seconds the camera swings in behind the direction the
            // player runs, at most controllerFollowSpeed radians per second (0 keeps the camera where the stick left it).
            // Left stick deflections up to controllerFollowDeadZone move the player without turning the camera
            float controllerFollowDelay = 1.0f;
            float controllerFollowSpeed = 0.0f;
            float controllerFollowDeadZone = 0.5f;

            // A jumps, false leaves the button to the application, e.g. while a modifier makes it another action
            bool controllerJump = true;

        private:
            float lookIdleTime = 0.0f;

        };

    }

}
