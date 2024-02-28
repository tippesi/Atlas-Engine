#include "AudioComponent.h"
#include "../../audio/AudioManager.h"

#include "../Scene.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            AudioComponent::AudioComponent(Scene *scene, const AudioComponent &that) {

                if (this != &that) {
                    *this = that;
                    // Need to create new stream
                    if (stream) {
                        stream = Audio::AudioManager::CreateStream(stream->data, 0.0f);
                    }   
                }

                this->scene = scene;

            }

            AudioComponent::AudioComponent(Scene* scene, const ResourceHandle<Audio::AudioData>& audioData,
                float falloffFactor, bool loop) : falloffFactor(falloffFactor), scene(scene) {

                stream = Audio::AudioManager::CreateStream(audioData, 0.0f, loop);

            }

            void AudioComponent::ChangeResource(ResourceHandle<Audio::AudioData> audioData) {

                AE_ASSERT(scene != nullptr && "Component needs to be added to entity before changing data");

                if (stream) {
                    scene->UnregisterResource(scene->registeredAudios, stream->data);

                    stream->ChangeData(audioData);
                }
                else {
                    stream = Audio::AudioManager::CreateStream(audioData, 0.0f);

                    stream->loop = true;
                }

                scene->RegisterResource(scene->registeredAudios, audioData);

            }

            void AudioComponent::Update(float deltaTime, const TransformComponent &transformComponent,
                vec3 listenerLocation, vec3 lastListenerLocation, vec3 listenerRight) {

                if (!stream)
                    return;

                const float epsilon = 0.00001f;

                deltaTime = std::max(deltaTime, epsilon);

                auto objectLocation = vec3(transformComponent.globalMatrix[3]);
                auto lastObjectLocation = vec3(transformComponent.lastGlobalMatrix[3]);

                auto distance = glm::max(epsilon, glm::distance(objectLocation, listenerLocation));

                // Use quick paths for "normal" powers and do nothing for the power=1 case
                auto powerDistance = distance;
                if (falloffPower == 2.0f)
                    powerDistance *= powerDistance;
                else if (falloffPower == 3.0f)
                    powerDistance *= powerDistance * powerDistance;
                else if (falloffPower != 1.0f)
                    powerDistance = powf(powerDistance, falloffPower);

                float distanceVolume = glm::min(1.0f, falloffFactor / powerDistance);

                auto audible = distanceVolume > cutoff;

                if (audible) {
                    auto mix = 0.0f;

                    if (distance == epsilon) {
                        mix = 0.5f;
                    }
                    else {
                        auto direction = glm::normalize(objectLocation - listenerLocation);
                        mix = 0.5f * glm::dot(direction, -listenerRight) + 0.5f;
                    }

                    auto lastDistance = glm::distance(lastObjectLocation, lastListenerLocation);

                    auto velocity = (lastDistance - distance) / std::max(deltaTime, epsilon);

                    // Avoids high pitch when there is no movement history
                    if (initialState) {
                        initialState = false;
                        velocity = 0.0f;
                    }

                    auto pitch = 1.0 + velocity / 333.3;
                    pitch = pitch >= 0.0 ? pitch : 0.0;

                    stream->SetVolume(distanceVolume * volume);

                    stream->SetChannelVolume(Audio::Channel::Left, mix);
                    stream->SetChannelVolume(Audio::Channel::Right, 1.0f - mix);

                    stream->SetPitch(pitch);
                }
                else {
                    stream->SetVolume(0.0f);
                }

            }

        }

    }

}