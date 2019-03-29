#include "AudioStream.h"

namespace Atlas {
    
    namespace Audio {

		double AudioStream::GetDuration() {

			return (double)data->data.size() * 2.0 / (double)(data->GetFrequency() * data->GetSampleSize());

		}

		double AudioStream::GetTime() {

			return 2.0 * progress * (double)data->GetChannelCount() / 
				(double)(data->GetFrequency() * data->GetSampleSize());

		}

		void AudioStream::SetTime(double time) {

			progress = time * (double)(data->GetFrequency() * data->GetSampleSize() / data->GetChannelCount()) / 2.0;
			progress = progress >= 0.0 ? progress : 0.0;

		}
        
		std::vector<int16_t> AudioStream::GetChunk(int32_t length) {

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

			data->ApplyFormat(spec);

		}

    }
    
}