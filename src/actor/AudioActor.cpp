#include "AudioActor.h"

namespace Atlas {

	namespace Actor {

		AudioActor::AudioActor(Audio::AudioData* data) : Actor(), Audio::AudioStream(data) {



		}

		AudioActor& AudioActor::operator=(const AudioActor& that) {

			if (this != &that) {

				AudioStream::operator=(that);

				aabb = that.aabb;
				transformedMatrix = that.transformedMatrix;
				SetMatrix(that.GetMatrix());

				cutoff = that.cutoff;

				leftChannelVolume = that.leftChannelVolume;
				rightChannelVolume = that.rightChannelVolume;

				velocity = that.velocity;
				cameraDistance = that.cameraDistance;

				audible = that.audible;
				init = that.init;

			}

			return *this;

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
			pitch = pitch >= 0.0 ? pitch : 0.0;
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

			// We don't want to devide by zero
			auto distance = glm::max(0.00001f, 
				glm::distance(vec3(transformedMatrix[3]), camera.GetLocation()));

			if (!init) {
				cameraDistance = distance;
				init = true;
			}

			float distanceVolume = glm::min(1.0f, 1.0f / (distance / 10.0f));

			audible = distanceVolume > cutoff;

			velocity = (cameraDistance - distance) / deltaTime;

			if (audible) {
				auto mix = 0.0f;

				if (distance == 0.00001f) {
					mix = 0.5f;
				}
				else {
					auto direction = glm::normalize(glm::vec3(transformedMatrix[3]) - camera.GetLocation());
					mix = 0.5f * glm::dot(direction, camera.right) + 0.5f;
				}

				leftChannelVolume = mix * distanceVolume;
				rightChannelVolume = (1.0f - mix) * distanceVolume;
			}

			cameraDistance = distance;

			matrixChanged = false;

		}

	}

}