#include "AudioActor.h"

namespace Atlas {

	namespace Actor {

		AudioActor::AudioActor(Audio::AudioData* data) : Actor(), Audio::AudioStream(data) {



		}

		std::vector<int16_t> AudioActor::GetChunk(int32_t length) {

			std::unique_lock<std::mutex> lock(mutex);
			auto audible = this->audible;
			auto velocity = this->velocity;
			lock.unlock();

			if (!audible)
				return std::vector<int16_t>(length, 0);

			// Maybe we should allow other wavespeeds (e.g. for underwater)
			auto pitch = 1.0 + velocity / 333.3;
			AudioStream::SetPitch(pitch);

			auto data = AudioStream::GetChunk(length);

			for (int32_t i = 0; i < length; i++) {
				if (i % 2) {
					data[i] = (int16_t)((float)data[i] * leftChannelVolume);
				}
				else {
					data[i] = (int16_t)((float)data[i] * rightChannelVolume);
				}
			}

			return data;

		}

		void AudioActor::Update(Camera camera, float deltaTime, 
			mat4 parentTransform, bool parentUpdate) {

			std::lock_guard<std::mutex> lock(mutex);

			vec3 lastLocation;

			if (matrixChanged || parentUpdate) {				

				lastLocation = vec3(transformedMatrix[3]);

				transformedMatrix = parentTransform * GetMatrix();

			}

			float distanceVolume = glm::min(1.0f, 1.0f / (glm::distance(vec3(transformedMatrix[3]), camera.GetLocation()) / 10.0f));

			audible = distanceVolume > cutoff;

			auto direction = glm::normalize(glm::vec3(transformedMatrix[3]) - camera.GetLocation());			

			if ((matrixChanged || parentUpdate) && audible) {

				auto actorDirection = vec3(transformedMatrix[3]) - lastLocation;
				auto distance = glm::length(actorDirection);

				auto abs = glm::dot(actorDirection, direction) > 0.0f ? -1.0f : 1.0f;

				velocity = distance / deltaTime * abs;

			}
			else {

				velocity = 0.0f;

			}

			if (audible) {
				float mix = 0.5f * glm::dot(direction, camera.right) + 0.5f;

				leftChannelVolume = (1.0f - mix) * distanceVolume;
				rightChannelVolume = mix * distanceVolume;

			}

			AtlasLog("%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f", leftChannelVolume, rightChannelVolume,
				glm::vec3(camera.viewMatrix[3]).x, glm::vec3(camera.viewMatrix[3]).y, glm::vec3(camera.viewMatrix[3]).z,
				velocity, distanceVolume);

			matrixChanged = false;

		}

	}

}