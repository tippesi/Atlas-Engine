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
                    Volume::AABB aabb, float falloffFactor = 1.0f) : Entity(entity, manager) {

                    AddComponent<AudioVolumeComponent>(audioData, aabb, falloffFactor);
                    AddComponent<TransformComponent>(mat4(1.0f), true);

                }

                AudioVolume(ECS::Entity entity, ECS::EntityManager* manager, ResourceHandle<Audio::AudioData> audioData,
                    Volume::AABB aabb, mat4 transform, float falloffFactor = 1.0f) : Entity(entity, manager) {

                    AddComponent<AudioVolumeComponent>(audioData, aabb, falloffFactor);
                    AddComponent<TransformComponent>(transform, true);

                }

            };

        }

    }

}