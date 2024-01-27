#pragma once

#include "System.h"
#include "events/EventManager.h"
#include "scene/components/CameraComponent.h"

namespace Atlas {

    namespace Input {

        class ControllerHandler {

        public:
            ControllerHandler();

            ControllerHandler(const ControllerHandler& that);

            ControllerHandler(Scene::Components::CameraComponent& camera, float sensibility, float speed, float reactivity,
                    float threshold, int32_t device = -1);

            ~ControllerHandler();

            ControllerHandler& operator=(const ControllerHandler& that);

            void Update(Scene::Components::CameraComponent& camera, float deltaTime);

            bool IsControllerAvailable();

            float sensibility = 1.5f;
            float speed = 7.0f;
            float reactivity = 6.0f;
            float threshold = 5000.0f;

        private:
            void RegisterEvents();

            void ControllerAxisEventHandler(Events::ControllerAxisEvent event);
            void ControllerButtonEventHandler(Events::ControllerButtonEvent event);
            void ControllerDeviceEventHandler(Events::ControllerDeviceEvent event);

            void DeepCopy(const ControllerHandler& that);

            vec2 leftStick = vec2(0.0f);
            vec2 rightStick = vec2(0.0f);

            float speedIncrease = 0.0f;

            vec3 linearVelocity = vec3(0.0f);
            vec2 angularVelocity = vec2(0.0f);

            vec3 interpolatedLinearVelocity = vec3(0.0f);
            vec2 interpolatedAngularVelocity = vec2(0.0f);

            int32_t controllerDevice = -1;

            int32_t controllerAxisEventHandle = -1;
            int32_t controllerButtonEventHandle = -1;
            int32_t controllerDeviceEventHandle = -1;

        };

    }

}