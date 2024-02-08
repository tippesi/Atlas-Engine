#include "AudioStream.h"

namespace Atlas {
    
    namespace Audio {

        AudioStream::AudioStream(ResourceHandle<Atlas::Audio::AudioData> data, float volume, bool loop)
            : data(data), volume(volume), loop(loop) {

            channelVolume.fill(1.0f);

        }

        void AudioStream::ChangeData(ResourceHandle<AudioData> data) {

            this->data = data;
            progress = 0.0;

        }

        double AudioStream::GetDuration() {

            std::lock_guard<std::mutex> lock(mutex);

            return (double)data->data.size() * 2.0 / (double)(data->GetFrequency() * data->GetSampleSize());

        }

        void AudioStream::SetTime(double time) {

            std::lock_guard<std::mutex> lock(mutex);

            if (!data.IsValid())
                return;

            progress = time * (double)(data->GetFrequency() * data->GetSampleSize() / data->GetChannelCount()) / 2.0;
            progress = progress >= 0.0 ? progress : 0.0;

        }

        double AudioStream::GetTime() const {

            std::lock_guard<std::mutex> lock(mutex);

            if (!data.IsValid())
                return 0.0;

            return 2.0 * progress * (double)data->GetChannelCount() / 
                (double)(data->GetFrequency() * data->GetSampleSize());

        }

        void AudioStream::SetVolume(float volume) {

            std::lock_guard<std::mutex> lock(mutex);

            this->volume = volume;

        }
        
        float AudioStream::GetVolume() const {

            std::lock_guard<std::mutex> lock(mutex);

            return volume;

        }

        void AudioStream::SetChannelVolume(Channel channel, float volume) {

            std::lock_guard<std::mutex> lock(mutex);

            this->channelVolume[channel] = volume;

        }

        float AudioStream::GetChannelVolume(Channel channel) const {

            std::lock_guard<std::mutex> lock(mutex);

            return this->channelVolume[channel];

        }

        void AudioStream::SetPitch(double pitch) {

            std::lock_guard<std::mutex> lock(mutex);

            this->pitch = pitch;

        }

        double AudioStream::GetPitch() const {

            std::lock_guard<std::mutex> lock(mutex);

            return pitch;

        }

        void AudioStream::Pause() {

            pause = true;

        }

        void AudioStream::Resume() {

            pause = false;

        }

        bool AudioStream::IsPaused() const {

            return pause;

        }

        bool AudioStream::IsValid() const {

            return data.IsLoaded() && data->isValid;

        }

        bool AudioStream::GetChunk(std::vector<int16_t>& chunk) {

            if (!data.IsLoaded())
                return false;

            std::lock_guard<std::mutex> lock(mutex);

            if (volume == 0.0f)
                return false;

            int32_t length = int32_t(chunk.size());
            std::memset(chunk.data(), 0, chunk.size() * 2);

            auto channels = (int32_t)data->GetChannelCount();
            length /= channels;

            auto sampleCount = (double)data->data.size() / (double)channels - 1.0;
            auto preMultipliedChannelVolume = channelVolume;

            for (int32_t i = 0; i < channels; i++) {
                preMultipliedChannelVolume[i] *= volume;
            }

            for (int32_t i = 0; i < length; i++) {

                if (progress >= sampleCount) {
                    if (loop)
                        progress = fmod(progress, sampleCount);
                    else
                        break;
                }

                if (pitch == 1.0) {
                    auto index = int32_t(progress) * channels;

                    for (int32_t j = 0; j < channels; j++) {
                        auto sample = float(data->data[index + j]);

                        sample *= preMultipliedChannelVolume[j];

                        chunk[i * channels + j] = (int16_t) sample;
                    }
                }
                else {
                    auto upperIndex = (int32_t) ceil(progress) * channels;
                    auto lowerIndex = (int32_t) floor(progress) * channels;

                    auto remainder = progress - floor(progress);

                    for (int32_t j = 0; j < channels; j++) {

                        auto sample = (double) data->data[lowerIndex + j] + remainder *
                            ((double) data->data[upperIndex + j] - (double) data->data[lowerIndex + j]);

                        sample *= preMultipliedChannelVolume[j];

                        chunk[i * channels + j] = (int16_t) sample;

                    }
                }

                progress += pitch;

            }

            return true;

        }

    }
    
}