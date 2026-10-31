#include "PlayerInput.h"
#include "ControllerMap.h"
#include "events/ControllerAxisEvent.h"
#include "events/ControllerButtonEvent.h"
#include "scene/Scene.h"
#include "common/MathHelper.h"

namespace Atlas {

    namespace Input {

        void PlayerInput::Update(Scene::Scene& scene, float deltaTime) {

            auto stick = [&](uint8_t axisX, uint8_t axisY) {

                // Rescaled behind the dead zone, such that small deflections start at zero
                auto value = vec2(ControllerMap::GetAxis(axisX), ControllerMap::GetAxis(axisY));
                auto length = glm::length(value);
                if (length <= controllerDeadZone)
                    return vec2(0.0f);
                return value / length * Helper::Math::Saturate((length - controllerDeadZone) / (1.0f - controllerDeadZone));

            };

            auto controller = useController && ControllerMap::IsConnected();

            auto playerSubset = scene.GetSubset<PlayerComponent, CameraComponent>();
            for (auto entity : playerSubset) {
                const auto& [player, camera] = playerSubset.Get(entity);

                if (useMouse)
                    mouseHandler.Update(camera, deltaTime);
                keyboardHandler.Update(camera, player, deltaTime);

                if (!controller)
                    continue;

                // Sticks are positive to the right and down, like the mouse movement
                auto look = stick(AE_CONTROLLERAXIS_RIGHTX, AE_CONTROLLERAXIS_RIGHTY);
                camera.rotation -= look * controllerLookSpeed * deltaTime;
                lookIdleTime = glm::length(look) > 0.0f ? 0.0f : lookIdleTime + deltaTime;

                auto move = stick(AE_CONTROLLERAXIS_LEFTX, AE_CONTROLLERAXIS_LEFTY);
                if (glm::length(move) > 0.0f) {
                    auto direction = Helper::Math::NormalizeOrZero(vec3(camera.direction.x, 0.0f, camera.direction.z));
                    auto velocity = direction * -move.y + camera.right * move.x;
                    auto run = glm::length(move) > 0.95f || ControllerMap::IsButtonPressed(AE_CONTROLLERBUTTON_LEFTSTICK);
                    player.SetInputVelocity(velocity * (run ? player.fastVelocity : player.slowVelocity));

                    // Running forward or sideways, not towards the camera
                    auto offset = std::atan2(-move.x, -move.y);
                    auto follow = Helper::Math::Saturate((glm::length(move) - controllerFollowDeadZone) /
                        (1.0f - controllerFollowDeadZone));
                    if (controllerFollowSpeed > 0.0f && follow > 0.0f && lookIdleTime > controllerFollowDelay &&
                        std::abs(offset) < 2.0f) {
                        auto step = controllerFollowSpeed * follow * deltaTime;
                        camera.rotation.x += glm::clamp(offset, -step, step);
                    }
                }

                if (controllerJump && ControllerMap::IsButtonPressed(AE_CONTROLLERBUTTON_A, false))
                    player.Jump();

                if (controllerCrouch && ControllerMap::IsButtonPressed(AE_CONTROLLERBUTTON_B))
                    player.Crouch();
            }

        }

    }

}
