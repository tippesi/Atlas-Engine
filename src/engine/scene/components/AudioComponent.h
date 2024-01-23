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
				AudioComponent(const AudioComponent& that) = delete;
                AudioComponent(AudioComponent&&) noexcept = default;
                explicit AudioComponent(ResourceHandle<Audio::AudioData> audioData,
                    float falloffFactor = 10.0f);

                AudioComponent& operator=(AudioComponent&&) noexcept = default;

                float falloffFactor = 10.0f;
                float cutoff = 0.0001f;

            private:
                void Update(float deltaTime, const TransformComponent& transformComponent,
                    vec3 listenerLocation, vec3 lastListenerLocation, vec3 listenerRight);

                Ref<Audio::AudioStream> stream;

                friend Scene;

			};

		}

	}

}