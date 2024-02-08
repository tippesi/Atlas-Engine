#include "Controller.h"
#include <math.h>

namespace Atlas {

    namespace Input {

        ControllerHandler::ControllerHandler() {

            RegisterEvents();

        }

        ControllerHandler::ControllerHandler(const ControllerHandler& that) {

            RegisterEvents();

            DeepCopy(that);

        }

        ControllerHandler::ControllerHandler(float sensibility, float speed, float reactivity,
                float threshold, int32_t device) :
                sensibility(sensibility), speed(speed), reactivity(reactivity),
                threshold(threshold), controllerDevice(device) {

            RegisterEvents();

        }

        ControllerHandler::~ControllerHandler() {

            Atlas::Events::EventManager::ControllerAxisEventDelegate.Unsubscribe(
                controllerAxisEventHandle
            );
            Atlas::Events::EventManager::ControllerButtonEventDelegate.Unsubscribe(
                controllerButtonEventHandle
            );
            Atlas::Events::EventManager::ControllerDeviceEventDelegate.Unsubscribe(
                controllerDeviceEventHandle
            );

        }

        ControllerHandler& ControllerHandler::operator=(const ControllerHandler& that) {

            if (this != &that) {

                DeepCopy(that);

            }

            return *this;

        }

        void ControllerHandler::Update(CameraComponent& camera, float deltaTime) {

            if (controllerDevice > -1) {

                linearVelocity = camera.direction * -leftStick.y * deltaTime * (speed + speedIncrease);
                linearVelocity += camera.right * leftStick.x * deltaTime * (speed + speedIncrease);

                angularVelocity = -rightStick * deltaTime * sensibility;

                float progress = glm::clamp(reactivity * deltaTime, 0.0f, 1.0f);

                interpolatedLinearVelocity = glm::mix(interpolatedLinearVelocity, linearVelocity, progress);
                interpolatedAngularVelocity = glm::mix(interpolatedAngularVelocity, angularVelocity, progress);

                camera.location += interpolatedLinearVelocity;
                camera.rotation += interpolatedAngularVelocity;

            }

        }

        bool ControllerHandler::IsControllerAvailable() {

            return controllerDevice > -1;

        }

        void ControllerHandler::RegisterEvents() {

            auto controllerAxisEventHandler = std::bind(&ControllerHandler::ControllerAxisEventHandler,
                this, std::placeholders::_1);
            controllerAxisEventHandle = Events::EventManager::ControllerAxisEventDelegate.Subscribe(
                controllerAxisEventHandler);

            auto controllerButtonEventHandler = std::bind(&ControllerHandler::ControllerButtonEventHandler,
                this, std::placeholders::_1);
            controllerButtonEventHandle = Events::EventManager::ControllerButtonEventDelegate.Subscribe(
                controllerButtonEventHandler);

            auto controllerDeviceEventHandler = std::bind(&ControllerHandler::ControllerDeviceEventHandler,
                this, std::placeholders::_1);
            controllerDeviceEventHandle = Events::EventManager::ControllerDeviceEventDelegate.Subscribe(
                controllerDeviceEventHandler);

        }

        void ControllerHandler::ControllerAxisEventHandler(Events::ControllerAxisEvent event) {

            if (event.device != controllerDevice)
                return;

            if (event.axis == AE_CONTROLLERAXIS_LEFTX) {
                leftStick.x = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
            }
            else if (event.axis == AE_CONTROLLERAXIS_LEFTY) {
                leftStick.y = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
            }
            if (event.axis == AE_CONTROLLERAXIS_RIGHTX) {
                rightStick.x = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
            }
            else if (event.axis == AE_CONTROLLERAXIS_RIGHTY) {
                rightStick.y = abs(event.value) > threshold ? (float)event.value / 32766.0f : 0.0f;
            }
            else if (event.axis == AE_CONTROLLERAXIS_RIGHTTRIGGER) {
                speedIncrease = 100.0f * (float)event.value / 32766.0f;
            }

        }

        void ControllerHandler::ControllerButtonEventHandler(Events::ControllerButtonEvent event) {

            if (event.device != controllerDevice)
                return;



        }

        void ControllerHandler::ControllerDeviceEventHandler(Events::ControllerDeviceEvent event) {

            if (event.type == AE_CONTROLLER_ADDED) {

                if (controllerDevice == -1) {
                    controllerDevice = event.device;
                }

            }
            else if (event.type == AE_CONTROLLER_REMOVED) {

                if (controllerDevice == event.device) {
                    controllerDevice = -1;
                }

            }

        }

        void ControllerHandler::DeepCopy(const ControllerHandler& that) {

            sensibility = that.sensibility;
            speed = that.speed;
            reactivity = that.reactivity;
            threshold = that.threshold;

            leftStick = that.leftStick;
            rightStick = that.rightStick;

            speedIncrease = that.speedIncrease;

            linearVelocity = that.linearVelocity;
            angularVelocity = that.angularVelocity;

            interpolatedLinearVelocity = that.interpolatedLinearVelocity;
            interpolatedAngularVelocity = that.interpolatedAngularVelocity;

            controllerDevice = that.controllerDevice;

        }

    }

}