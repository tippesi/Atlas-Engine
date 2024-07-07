#pragma once

#include "../Entity.h"

#include "TransformComponent.h"

#include "../../System.h"
#include "../../audio/AudioStream.h"

namespace Atlas {

    namespace Scene {

        class Scene;

        namespace Components {

            class AudioVolumeComponent {

            public:
                AudioVolumeComponent() = default;
                explicit AudioVolumeComponent(Scene* scene) : scene(scene) {}
                explicit AudioVolumeComponent(Scene* scene, const AudioVolumeComponent& that);
                explicit AudioVolumeComponent(Scene* scene, const ResourceHandle<Audio::AudioData>& audioData,
                    Volume::AABB aabb = Volume::AABB(vec3(-1.0f), vec3(1.0f)), float falloffFactor = 1.0f);

                void ChangeResource(ResourceHandle<Audio::AudioData> audioData);

                Volume::AABB GetTransformedAABB() const;

                float falloffFactor = 0.5f;
                float falloffPower = 2.0f;
                float cutoff = 0.0001f;

                float volume = 1.0f;

                Volume::AABB aabb = Volume::AABB(vec3(-1.0f), vec3(1.0f));

                Ref<Audio::AudioStream> stream = nullptr;

                bool permanentPlay = true;

            private:
                void Update(const TransformComponent& transformComponent, vec3 listenerLocation);

                Scene* scene = nullptr;

                Volume::AABB transformedAABB;

                friend Scene;

            };

        }

    }

}