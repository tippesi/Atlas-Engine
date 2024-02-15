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

            linearVelocity = camera.direction * movement.x * speed;
            linearVelocity += camera.right * movement.y * speed;

            // linearVelocity.y += movement.z * speed;
            linearVelocity.y = 0.0f;

            float progress = glm::clamp(reactivity, 0.0f, 1.0f);

            interpolatedLinearVelocity = glm::mix(interpolatedLinearVelocity, linearVelocity, progress);

            auto playerUp = player.GetUp();

            auto newVelocity = vec3(0.0f);
            auto groundVelocity = player.GetGroundVelocity();
            if (player.IsOnGround()) {
                newVelocity += groundVelocity;
                if (jump) {
                    newVelocity += playerUp * speed;
                }
                newVelocity += interpolatedLinearVelocity;
            }
            else {
                newVelocity += player.GetLinearVelocity();
                // Add reduced velocity such that jumping doesn't feel weird
                newVelocity += interpolatedLinearVelocity * 0.01f;
            }

            auto gravity = vec3(0.0f, -9.81f, 0.0f);
            newVelocity += playerUp * gravity * deltaTime;

            player.SetLinearVelocity(newVelocity);

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