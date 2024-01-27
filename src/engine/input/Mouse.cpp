#include "Mouse.h"

#include <stdio.h>
#include <stdlib.h>

namespace Atlas {

    namespace Input {

        MouseHandler::MouseHandler() {

            RegisterEvents();

        }

        MouseHandler::MouseHandler(const MouseHandler& that) {

            RegisterEvents();

            DeepCopy(that);

        }

        MouseHandler::MouseHandler(Scene::Components::CameraComponent& camera, float sensibility, float reactivity, bool hideMouse) :
                sensibility(sensibility), reactivity(reactivity), hideMouse(hideMouse) {

            RegisterEvents();

        }

        MouseHandler::~MouseHandler() {

            Atlas::Events::EventManager::MouseMotionEventDelegate.Unsubscribe(
                mouseMotionEventHandle
            );
            Atlas::Events::EventManager::MouseButtonEventDelegate.Unsubscribe(
                mouseButtonEventHandle
            );

        }

        MouseHandler& MouseHandler::operator=(const MouseHandler& that) {

            if (this != &that) {

                DeepCopy(that);

            }

            return *this;

        }

        void MouseHandler::Update(Scene::Components::CameraComponent& camera, float deltaTime) {

            float progress = glm::clamp(reactivity * deltaTime, 0.0f, 1.0f);

            interpolatedAngularVelocity = glm::mix(interpolatedAngularVelocity, angularVelocity, progress);
            camera.rotation += interpolatedAngularVelocity;

            angularVelocity = vec2(0.0f);

        }

        void MouseHandler::SetActivationButton(uint8_t mouseButton) {

            activationButton = mouseButton;

        }

        void MouseHandler::HideMouse() {

            hideMouse = true;
            SDL_ShowCursor(0);

        }

        void MouseHandler::ShowMouse() {

            hideMouse = false;
            SDL_ShowCursor(1);

        }

        void MouseHandler::RegisterEvents() {

            auto mouseMotionEventHandler = std::bind(&MouseHandler::MouseMotionEventHandler, this, std::placeholders::_1);
            mouseMotionEventHandle = Events::EventManager::MouseMotionEventDelegate.Subscribe(mouseMotionEventHandler);

            auto mouseButtonEventHandler = std::bind(&MouseHandler::MouseButtonEventHandler, this, std::placeholders::_1);
            mouseButtonEventHandle = Events::EventManager::MouseButtonEventDelegate.Subscribe(mouseButtonEventHandler);

        }

        void MouseHandler::MouseMotionEventHandler(Events::MouseMotionEvent event) {

            if (event.windowID == 0 || lock)
                return;

            if (hideMouse) {



            }
            else {

                if (!activationButtonDown)
                    return;

                angularVelocity = glm::vec2(-((float)event.dx), -((float)event.dy)) * sensibility * 0.001f;

            }

        }

        void MouseHandler::MouseButtonEventHandler(Events::MouseButtonEvent event) {

            if (event.windowID == 0)
                return;

            if (event.button == activationButton && event.state == AE_BUTTON_PRESSED)
                activationButtonDown = true;

            if (event.button == activationButton && event.state == AE_BUTTON_RELEASED)
                activationButtonDown = false;

        }

        void MouseHandler::DeepCopy(const MouseHandler& that) {

            sensibility = that.sensibility;
            reactivity = that.reactivity;

            lock = that.lock;
            hideMouse = that.hideMouse;

            activationButtonDown = that.activationButtonDown;
            activationButton = that.activationButton;

            angularVelocity = that.angularVelocity;
            interpolatedAngularVelocity = that.interpolatedAngularVelocity;

        }

    }

}