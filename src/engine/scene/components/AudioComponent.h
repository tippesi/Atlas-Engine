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
                explicit AudioComponent(ResourceHandle<Audio::AudioData> audioData,
                    float falloffFactor = 1.0f, bool loop = false);

                float falloffFactor = 1.0f;
                float cutoff = 0.0001f;

                Ref<Audio::AudioStream> stream;

            private:
                void Update(float deltaTime, const TransformComponent& transformComponent,
                    vec3 listenerLocation, vec3 lastListenerLocation, vec3 listenerRight);

                bool initialState = true;

                friend Scene;

			};

		}

	}

}