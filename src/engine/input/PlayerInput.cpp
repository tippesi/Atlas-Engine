#include "PlayerInput.h"
#include "scene/Scene.h"

namespace Atlas {

    namespace Input {

        void PlayerInput::Update(Scene::Scene& scene, float deltaTime) {

            auto playerSubset = scene.GetSubset<PlayerComponent, CameraComponent>();
            for (auto entity : playerSubset) {
                const auto& [player, camera] = playerSubset.Get(entity);

                if (useMouse)
                    mouseHandler.Update(camera, deltaTime);
                keyboardHandler.Update(camera, player, deltaTime);
            }

        }

    }

}
