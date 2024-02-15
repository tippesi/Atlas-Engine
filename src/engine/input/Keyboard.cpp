#include "Keyboard.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

namespace Atlas {

    namespace Input {

        KeyboardHandler::KeyboardHandler() {

            RegisterEvent();

        }

        KeyboardHandler::KeyboardHandler(const KeyboardHandler& that) {

            RegisterEvent();

            DeepCopy(that);

        }

        KeyboardHandler::KeyboardHandler(float speed, float reactivity) :
                speed(speed), reactivity(reactivity) {

            RegisterEvent();
            
        }

        KeyboardHandler::~KeyboardHandler() {

            Events::EventManager::KeyboardEventDelegate.Unsubscribe(eventHandle);

        }

        KeyboardHandler& KeyboardHandler::operator=(const KeyboardHandler& that) {

            if (this != &that) {

                DeepCopy(that);

            }

            return *this;

        }

        void KeyboardHandler::Update(CameraComponent& camera, float deltaTime) {

            linearVelocity = camera.direction * movement.x * speed;
            linearVelocity += camera.right * movement.y * speed;

            linearVelocity.y += movement.z * speed;

            float progress = glm::clamp(reactivity * deltaTime, 0.0f, 1.0f);

            interpolatedLinearVelocity = glm::mix(interpolatedLinearVelocity, linearVelocity, progress);

            camera.location += interpolatedLinearVelocity * deltaTime;

        }

        void KeyboardHandler::Update(const CameraComponent& camera, PlayerComponent& player, float deltaTime) {

            linearVelocity = camera.direction * movement.x;
            linearVelocity += camera.right * movement.y;

            // linearVelocity.y += movement.z * speed;
            linearVelocity.y = 0.0f;

            if (glm::length(linearVelocity) > 0.0f)
                linearVelocity = glm::normalize(linearVelocity);
            float progress = glm::clamp(reactivity, 0.0f, 1.0f);

            interpolatedLinearVelocity = glm::mix(interpolatedLinearVelocity, linearVelocity, progress);

            auto velocity = fast ? interpolatedLinearVelocity * player.fastVelocity : 
                interpolatedLinearVelocity * player.slowVelocity;

            player.SetInputVelocity(velocity);
            if (jump)
                player.Jump();

        }

        void KeyboardHandler::RegisterEvent() {

            auto keyboardEventHandler = std::bind(&KeyboardHandler::KeyboardEventHandler, this, std::placeholders::_1);
            eventHandle = Events::EventManager::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

        }

        void KeyboardHandler::KeyboardEventHandler(Events::KeyboardEvent event) {

            if (event.keyCode == AE_KEY_W && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.x += 1.0f;
            }

            if (event.keyCode == AE_KEY_W && event.state == AE_BUTTON_RELEASED) {
                movement.x -= 1.0f;
            }

            if (event.keyCode == AE_KEY_S && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.x -= 1.0f;
            }

            if (event.keyCode == AE_KEY_S && event.state == AE_BUTTON_RELEASED) {
                movement.x += 1.0f;
            }

            if (event.keyCode == AE_KEY_D && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.y += 1.0f;
            }

            if (event.keyCode == AE_KEY_D && event.state == AE_BUTTON_RELEASED) {
                movement.y -= 1.0f;
            }

            if (event.keyCode == AE_KEY_A && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.y -= 1.0f;
            }

            if (event.keyCode == AE_KEY_A && event.state == AE_BUTTON_RELEASED) {
                movement.y += 1.0f;
            }


            if (event.keyCode == AE_KEY_E && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.z += 1.0f;
            }

            if (event.keyCode == AE_KEY_E && event.state == AE_BUTTON_RELEASED) {
                movement.z -= 1.0f;
            }


            if (event.keyCode == AE_KEY_Q && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.z -= 1.0f;
            }

            if (event.keyCode == AE_KEY_Q && event.state == AE_BUTTON_RELEASED) {
                movement.z += 1.0f;
            }

            if (event.keyCode == AE_KEY_SPACE && event.state == AE_BUTTON_PRESSED) {
                jump = true;
            }

            if (event.keyCode == AE_KEY_SPACE && event.state == AE_BUTTON_RELEASED) {
                jump = false;
            }

            if (event.keyCode == AE_KEY_LSHIFT && event.state == AE_BUTTON_PRESSED) {
                fast = true;
            }

            if (event.keyCode == AE_KEY_LSHIFT && event.state == AE_BUTTON_RELEASED) {
                fast = false;
            }
            

        }

        void KeyboardHandler::DeepCopy(const KeyboardHandler& that) {

            speed = that.speed;
            reactivity = that.reactivity;

            linearVelocity = that.linearVelocity;
            interpolatedLinearVelocity = that.interpolatedLinearVelocity;

            movement = that.movement;

        }

    }

}