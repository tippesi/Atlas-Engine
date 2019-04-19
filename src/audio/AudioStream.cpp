#include "AudioStream.h"

namespace Atlas {
    
    namespace Audio {

		AudioStream& AudioStream::operator=(const AudioStream& that) {

		    if (this != &that) {

                std::lock(mutex, that.mutex);
                std::lock_guard<std::mutex> lock_this(mutex, std::adopt_lock);
                std::lock_guard<std::mutex> lock_that(that.mutex, std::adopt_lock);

                this->loop = that.loop;

                this->progress = that.progress;

                this->volume = that.volume;
                this->pitch = that.pitch;

                this->data = that.data;

            }

			return *this;

		}

		double AudioStream::GetDuration() {

			std::lock_guard<std::mutex> lock(mutex);

			return (double)data->data.size() * 2.0 / (double)(data->GetFrequency() * data->GetSampleSize());

		}

		void AudioStream::SetTime(double time) {

			std::lock_guard<std::mutex> lock(mutex);

			progress = time * (double)(data->GetFrequency() * data->GetSampleSize() / data->GetChannelCount()) / 2.0;
			progress = progress >= 0.0 ? progress : 0.0;

		}

		double AudioStream::GetTime() {

			std::lock_guard<std::mutex> lock(mutex);

			return 2.0 * progress * (double)data->GetChannelCount() / 
				(double)(data->GetFrequency() * data->GetSampleSize());

		}

		void AudioStream::SetVolume(float volume) {

			std::lock_guard<std::mutex> lock(mutex);

			this->volume = volume;

		}
        
		float AudioStream::GetVolume() {

			std::lock_guard<std::mutex> lock(mutex);

			return volume;

		}

		void AudioStream::SetPitch(double pitch) {

			std::lock_guard<std::mutex> lock(mutex);

			this->pitch = pitch;

		}

		double AudioStream::GetPitch() {

			std::lock_guard<std::mutex> lock(mutex);

			return pitch;

		}

		std::vector<int16_t> AudioStream::GetChunk(int32_t length) {

			std::lock_guard<std::mutex> lock(mutex);

			std::vector<int16_t> chunk(length);

			std::memset(chunk.data(), 0, chunk.size() * 2);

			auto channels = (int32_t)data->GetChannelCount();

			length /= channels;

			auto sampleCount = (double)data->data.size() / (double)channels - 1.0;

			for (int32_t i = 0; i < length; i++) {

				if (progress >= sampleCount) {
					if (loop)
						progress = fmod(progress, sampleCount);
					else
						break;
				}

				auto upperIndex = (int32_t)ceil(progress) * channels;
				auto lowerIndex = (int32_t)floor(progress) * channels;

				auto remainder = progress - floor(progress);
				
				for (int32_t j = 0; j < channels; j++) {

					auto sample = (double)data->data[lowerIndex + j] + remainder *
						((double)data->data[upperIndex + j] - (double)data->data[lowerIndex + j]);

					sample *= volume;

					chunk[i * channels + j] = (int16_t)sample;

				}

				progress += pitch;

			}

			return chunk;

		}

		void AudioStream::ApplyFormat(SDL_AudioSpec& spec) {

			std::lock_guard<std::mutex> lock(mutex);

			data->ApplyFormat(spec);

		}

    }
    
}