#include "AudioVolumeComponent.h"

#include "../../audio/AudioManager.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            AudioVolumeComponent::AudioVolumeComponent(ResourceHandle<Audio::AudioData> audioData,
                Volume::AABB aabb, float falloffFactor) : aabb(aabb), falloffFactor(falloffFactor) {

                stream = Audio::AudioManager::CreateStream(audioData, 0.0f);

                stream->loop = true;

            }

            void AudioVolumeComponent::Update(const TransformComponent &transformComponent, vec3 listenerLocation) {

                const float epsilon = 0.00001f;

                auto distance = glm::max(epsilon, aabb.Transform(transformComponent.globalMatrix).GetDistance(listenerLocation));

                float distanceVolume = glm::min(1.0f, falloffFactor / distance);

                auto audible = distanceVolume > cutoff;

                if (audible) {
                    stream->SetVolume(distanceVolume);
                }
                else {
                    stream->SetVolume(0.0f);
                }

            }

        }

    }

}