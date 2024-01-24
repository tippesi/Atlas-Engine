#pragma once

#include "../Entity.h"

#include "../components/AudioVolumeComponent.h"
#include "../components/TransformComponent.h"

namespace Atlas {

    namespace Scene {

        namespace Prefabs {

            class AudioVolume : public Entity {

            public:
                AudioVolume(ECS::Entity entity, ECS::EntityManager* manager, ResourceHandle<Audio::AudioData> audioData,
                    float falloffFactor = 10.0f) : Entity(entity, manager) {

                    AddComponent<Components::AudioVolumeComponent>(audioData, falloffFactor);
                    AddComponent<Components::TransformComponent>(mat4(1.0f), true);

                }

                AudioVolume(ECS::Entity entity, ECS::EntityManager* manager, ResourceHandle<Audio::AudioData> audioData,
                    mat4 transform, float falloffFactor = 10.0f) : Entity(entity, manager) {

                    AddComponent<Components::AudioVolumeComponent>(audioData, falloffFactor);
                    AddComponent<Components::TransformComponent>(transform, true);

                }

            };

        }

    }

}