#include "AudioVolumeComponent.h"

#include "../../audio/AudioManager.h"

#include "../Scene.h"

namespace Atlas {

    namespace Scene {

        namespace Components {

            AudioVolumeComponent::AudioVolumeComponent(Scene *scene, const AudioVolumeComponent &that) {

                if (this != &that) {
                    *this = that;
                    // Need to create new stream
                    if (stream) {
                        stream = Audio::AudioManager::CreateStream(stream->data, 0.0f);
                    }   
                }

                this->scene = scene;

            }

            AudioVolumeComponent::AudioVolumeComponent(Scene* scene, const ResourceHandle<Audio::AudioData>& audioData,
                Volume::AABB aabb, float falloffFactor) : aabb(aabb), falloffFactor(falloffFactor), scene(scene) {

                stream = Audio::AudioManager::CreateStream(audioData, 0.0f);

                stream->loop = true;

            }

            void AudioVolumeComponent::ChangeResource(ResourceHandle<Audio::AudioData> audioData) {

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

            Volume::AABB AudioVolumeComponent::GetTransformedAABB() const {

                return transformedAABB;

            }

            void AudioVolumeComponent::Update(const TransformComponent &transformComponent, vec3 listenerLocation) {

                if (!stream)
                    return;

                if (scene->physicsWorld->pauseSimulation && !permanentPlay) {
                    stream->SetVolume(0.0f);
                    return;
                }

                const float epsilon = 0.00001f;

                transformedAABB = aabb.Transform(transformComponent.globalMatrix);
                auto distance = glm::max(epsilon, transformedAABB.GetDistance(listenerLocation));

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
                    stream->SetVolume(distanceVolume * volume);
                }
                else {
                    stream->SetVolume(0.0f);
                }

            }

        }

    }

}