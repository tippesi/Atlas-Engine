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
                AudioVolumeComponent(const AudioVolumeComponent& that) = delete;
                AudioVolumeComponent(AudioVolumeComponent&&) noexcept = default;
                explicit AudioVolumeComponent(ResourceHandle<Audio::AudioData> audioData,
                    Volume::AABB aabb, float falloffFactor = 1.0f);

                AudioVolumeComponent& operator=(AudioVolumeComponent&&) noexcept = default;

                Volume::AABB aabb;

                float falloffFactor = 1.0f;
                float cutoff = 0.0001f;

            private:
                void Update(const TransformComponent& transformComponent, vec3 listenerLocation);

                Ref<Audio::AudioStream> stream;

                friend Scene;

            };

        }

    }

}