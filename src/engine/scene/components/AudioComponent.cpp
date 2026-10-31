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

                if ((scene->physicsWorld->pauseSimulation || scene->paused) && !permanentPlay) {
                    stream->SetVolume(0.0f);
                    return;
                }

                const float epsilon = 0.00001f;

                deltaTime = std::max(deltaTime, epsilon);

                auto objectLocation = vec3(transformComponent.globalMatrix[3]);
                auto lastObjectLocation = vec3(transformComponent.lastGlobalMatrix[3]);

                auto audible = stream->Spatialize(objectLocation, listenerLocation, listenerRight, volume, falloffFactor,
                    falloffPower, cutoff);

                if (audible) {
                    auto distance = glm::max(epsilon, glm::distance(objectLocation, listenerLocation));
                    auto lastDistance = glm::distance(lastObjectLocation, lastListenerLocation);

                    auto velocity = (lastDistance - distance) / std::max(deltaTime, epsilon);

                    // Avoids high pitch when there is no movement history
                    if (initialState) {
                        initialState = false;
                        velocity = 0.0f;
                    }

                    auto pitch = 1.0 + velocity / 333.3;
                    pitch = pitch >= 0.0 ? pitch : 0.0;

                    stream->SetPitch(pitch);
                }

            }

        }

    }

}