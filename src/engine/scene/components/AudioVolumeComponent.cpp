#include "AudioVolumeComponent.h"

#include "../../audio/AudioManager.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            AudioVolumeComponent::AudioVolumeComponent(ResourceHandle<Audio::AudioData> audioData,
                float falloffFactor) : falloffFactor(falloffFactor) {

                stream = Audio::AudioManager::CreateStream(audioData);

                stream->loop = true;

            }

            void AudioVolumeComponent::Update(const TransformComponent &transformComponent, vec3 listenerLocation) {

                const float epsilon = 0.00001f;

                auto objectLocation = vec3(transformComponent.globalMatrix[3]);
                auto distance = glm::max(epsilon, glm::distance(objectLocation, listenerLocation));

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