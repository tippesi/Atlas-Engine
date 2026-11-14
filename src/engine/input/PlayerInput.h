#pragma once

#include "System.h"
#include "Keyboard.h"
#include "Mouse.h"
#include "events/ControllerButtonEvent.h"

namespace Atlas {

    namespace Scene {
        class Scene;
    }

    namespace Input {

        /**
         * Controls the players of a scene with keyboard and mouse: W, A, S and D move a player relative to
         * the direction of its camera, the mouse rotates the camera, the keys to jump, run and crouch are set in the
         * keyboard handler (Space, Left Shift and C by default). Game controllers move with the left stick (fully
         * deflected or with the run button held to run), look with the right stick, jump with the jump button and
         * crouch with the crouch button.
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

            // The jump and crouch buttons, false leaves the button to the application, e.g. while it is another action
            bool controllerJump = true;
            bool controllerCrouch = true;
            int32_t jumpButton = AE_CONTROLLERBUTTON_A;
            int32_t crouchButton = AE_CONTROLLERBUTTON_B;
            // Held runs, -1 runs only at full deflection
            int32_t runButton = AE_CONTROLLERBUTTON_LEFTSTICK;

        private:
            float lookIdleTime = 0.0f;

        };

    }

}
