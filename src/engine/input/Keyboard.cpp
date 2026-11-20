#include "Keyboard.h"
#include "common/MathHelper.h"

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

            float progress = Helper::Math::Saturate(reactivity * deltaTime);

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
            float progress = Helper::Math::Saturate(reactivity);

            interpolatedLinearVelocity = glm::mix(interpolatedLinearVelocity, linearVelocity, progress);

            auto velocity = fast != alwaysRun ? interpolatedLinearVelocity * player.fastVelocity : 
                interpolatedLinearVelocity * player.slowVelocity;

            player.SetInputVelocity(velocity);
            if (jump)
                player.Jump();

            if (slide)
                player.Slide();

            if (crouch)
                player.Crouch();

        }

        void KeyboardHandler::RegisterEvent() {

            auto keyboardEventHandler = std::bind(&KeyboardHandler::KeyboardEventHandler, this, std::placeholders::_1);
            eventHandle = Events::EventManager::KeyboardEventDelegate.Subscribe(keyboardEventHandler);

        }

        void KeyboardHandler::KeyboardEventHandler(Events::KeyboardEvent event) {

            if (event.keyCode == Keycode::KeyW && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.x += 1.0f;
            }

            if (event.keyCode == Keycode::KeyW && event.state == AE_BUTTON_RELEASED) {
                movement.x -= 1.0f;
            }

            if (event.keyCode == Keycode::KeyS && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.x -= 1.0f;
            }

            if (event.keyCode == Keycode::KeyS && event.state == AE_BUTTON_RELEASED) {
                movement.x += 1.0f;
            }

            if (event.keyCode == Keycode::KeyD && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.y += 1.0f;
            }

            if (event.keyCode == Keycode::KeyD && event.state == AE_BUTTON_RELEASED) {
                movement.y -= 1.0f;
            }

            if (event.keyCode == Keycode::KeyA && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.y -= 1.0f;
            }

            if (event.keyCode == Keycode::KeyA && event.state == AE_BUTTON_RELEASED) {
                movement.y += 1.0f;
            }


            if (event.keyCode == Keycode::KeyE && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.z += 1.0f;
            }

            if (event.keyCode == Keycode::KeyE && event.state == AE_BUTTON_RELEASED) {
                movement.z -= 1.0f;
            }


            if (event.keyCode == Keycode::KeyQ && event.state == AE_BUTTON_PRESSED && !event.repeat) {
                movement.z -= 1.0f;
            }

            if (event.keyCode == Keycode::KeyQ && event.state == AE_BUTTON_RELEASED) {
                movement.z += 1.0f;
            }

            if (event.keyCode == Keycode::KeyUnknown)
                return;

            if (event.keyCode == jumpKey)
                jump = event.state == AE_BUTTON_PRESSED;

            if (event.keyCode == runKey)
                fast = event.state == AE_BUTTON_PRESSED;

            if (event.keyCode == slideKey)
                slide = event.state == AE_BUTTON_PRESSED;

            if (event.keyCode == crouchKey)
                crouch = event.state == AE_BUTTON_PRESSED;
           
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