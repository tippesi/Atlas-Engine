#pragma once

#include "System.h"
#include "scene/components/CameraComponent.h"
#include "events/EventManager.h"

namespace Atlas {

    namespace Input {

        class MouseHandler {

        public:
            MouseHandler();

            MouseHandler(const MouseHandler& that);

            MouseHandler(Scene::Components::CameraComponent& camera, float sensibility, float reactivity, bool hideMouse = false);

            ~MouseHandler();

            MouseHandler& operator=(const MouseHandler& that);

            void Update(Scene::Components::CameraComponent& camera, float deltaTime);

            void SetActivationButton(uint8_t mouseButton);

            void HideMouse();

            void ShowMouse();

            float sensibility = 1.5f;
            float reactivity = 6.0f;

            bool lock = false;
            bool hideMouse = false;

        private:
            void RegisterEvents();

            void MouseMotionEventHandler(Events::MouseMotionEvent event);
            void MouseButtonEventHandler(Events::MouseButtonEvent event);

            void DeepCopy(const MouseHandler& that);

            bool activationButtonDown = false;
            uint8_t activationButton = AE_MOUSEBUTTON_LEFT;

            vec2 angularVelocity = vec2(0.0f);
            vec2 interpolatedAngularVelocity = vec2(0.0f);

            int32_t mouseMotionEventHandle = -1;
            int32_t mouseButtonEventHandle = -1;

        };

    }

}