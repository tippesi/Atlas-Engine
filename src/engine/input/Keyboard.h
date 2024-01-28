#pragma once

#include "System.h"
#include "events/EventManager.h"
#include "scene/components/CameraComponent.h"

namespace Atlas {

    namespace Input {

        class KeyboardHandler {

        public:
            KeyboardHandler();

            KeyboardHandler(const KeyboardHandler& that);

            KeyboardHandler(CameraComponent& camera, float speed, float reactivity);

            ~KeyboardHandler();

            KeyboardHandler& operator=(const KeyboardHandler& that);

            void Update(CameraComponent& camera, float deltaTime);

            float speed = 7.0f;
            float reactivity = 6.0f;

        private:
            void RegisterEvent();

            void KeyboardEventHandler(Events::KeyboardEvent event);

            void DeepCopy(const KeyboardHandler& that);

            vec3 linearVelocity = vec3(0.0f);
            vec3 interpolatedLinearVelocity = vec3(0.0f);

            vec3 movement = vec3(0.0f);

            int32_t eventHandle = -1;

        };

    }

}