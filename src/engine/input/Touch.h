#pragma once

#include "System.h"
#include "events/EventManager.h"
#include "scene/components/CameraComponent.h"

namespace Atlas {

    namespace Input {

        class TouchHandler {

        public:
            TouchHandler();

            TouchHandler(const TouchHandler& that);

            TouchHandler(Scene::Components::CameraComponent& camera, float sensibility, float speed, float reactivity);

            ~TouchHandler();

            TouchHandler& operator=(const TouchHandler& that);

            void Update(Scene::Components::CameraComponent& camera, float deltaTime);

            float sensibility = 1.5f;
            float speed = 7.0f;
            float reactivity = 6.0f;

        private:
            void RegisterEvent();

            void TouchEventHandler(Events::TouchEvent event);

            void DeepCopy(const TouchHandler& that);

            struct Finger {
                vec2 position;
                int64_t ID;
            };

            struct Finger leftFinger = { vec2(0.0f), -1 };
            struct Finger rightFinger = { vec2(0.0f), -1 };

            vec3 linearVelocity = vec3(0.0f);
            vec2 angularVelocity = vec2(0.0f);

            vec3 interpolatedLinearVelocity = vec3(0.0f);
            vec2 interpolatedAngularVelocity = vec2(0.0f);

            int32_t eventHandle = -1;

        };

    }

}