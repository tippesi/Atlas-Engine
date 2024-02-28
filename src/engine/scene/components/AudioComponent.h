#pragma once

#include "../Entity.h"

#include "TransformComponent.h"

#include "../../System.h"
#include "../../audio/AudioStream.h"

namespace Atlas {

	namespace Scene {

        class Scene;

		namespace Components {

			class AudioComponent {

			public:
				AudioComponent() = default;
                explicit AudioComponent(Scene* scene) : scene(scene) {}
                explicit AudioComponent(Scene* scene, const AudioComponent& that);
                explicit AudioComponent(Scene* scene, ResourceHandle<Audio::AudioData>& audioData,
                    float falloffFactor = 1.0f, bool loop = false);

                void ChangeResource(ResourceHandle<Audio::AudioData> audioData);

                float falloffFactor = 0.5f;
                float falloffPower = 2.0f;
                float cutoff = 0.001f;

                float volume = 1.0f;

                Ref<Audio::AudioStream> stream = nullptr;

            private:
                void Update(float deltaTime, const TransformComponent& transformComponent,
                    vec3 listenerLocation, vec3 lastListenerLocation, vec3 listenerRight);

                Scene* scene = nullptr;

                bool initialState = true;

                friend Scene;

			};

		}

	}

}