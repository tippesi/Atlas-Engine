#include "AudioStream.h"
#include "AudioManager.h"
#include "../common/MathHelper.h"

namespace Atlas {
    
    namespace Audio {

        AudioStream::AudioStream(ResourceHandle<Atlas::Audio::AudioData> data, float volume, bool loop)
            : loop(loop), data(data), volume(volume) {

            channelVolume.fill(1.0f);

        }

        void AudioStream::ChangeData(ResourceHandle<AudioData> data) {

            this->data = data;
            progress = 0.0;
            finished = false;

        }

        double AudioStream::GetDuration() {

            std::lock_guard<std::mutex> lock(mutex);

            return (double)data->data.size() * 2.0 / (double)(data->GetFrequency() * data->GetSampleSize());

        }

        void AudioStream::SetTime(double time) {

            std::lock_guard<std::mutex> lock(mutex);

            if (!data.IsLoaded())
                return;

            progress = time * (double)(data->GetFrequency() * data->GetSampleSize() / data->GetChannelCount()) / 2.0;
            progress = progress >= 0.0 ? progress : 0.0;
            finished = false;

        }

        double AudioStream::GetTime() const {

            std::lock_guard<std::mutex> lock(mutex);

            if (!data.IsLoaded())
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

        void AudioStream::SetPan(float pan) {

            std::lock_guard<std::mutex> lock(mutex);

            auto mix = Helper::Math::Saturate(0.5f - 0.5f * pan);
            channelVolume[Left] = mix;
            channelVolume[Right] = 1.0f - mix;

        }

        bool AudioStream::Spatialize(vec3 source, vec3 listener, vec3 listenerRight, float volume, float falloffFactor,
            float falloffPower, float cutoff) {

            const float epsilon = 0.00001f;
            auto distance = glm::max(epsilon, glm::distance(source, listener));

            // Use quick paths for "normal" powers and do nothing for the power=1 case
            auto powerDistance = distance;
            if (falloffPower == 2.0f)
                powerDistance *= powerDistance;
            else if (falloffPower == 3.0f)
                powerDistance *= powerDistance * powerDistance;
            else if (falloffPower != 1.0f)
                powerDistance = powf(powerDistance, falloffPower);

            auto distanceVolume = glm::min(1.0f, falloffFactor / powerDistance);
            if (distanceVolume <= cutoff) {
                SetVolume(0.0f);
                return false;
            }

            // Sources at the listener (e.g. music attached to the camera) have no stable direction,
            // so panning fades in over the first meter instead of flipping between the channels
            auto direction = Helper::Math::NormalizeOrZero(source - listener);
            auto panning = Helper::Math::Saturate(distance);
            auto mix = 0.5f * panning * glm::dot(direction, -listenerRight) + 0.5f;

            std::lock_guard<std::mutex> lock(mutex);
            this->volume = distanceVolume * volume;
            channelVolume[Left] = mix;
            channelVolume[Right] = 1.0f - mix;
            return true;

        }

        void AudioStream::SetPitch(double pitch) {

            std::lock_guard<std::mutex> lock(mutex);

            this->pitch = pitch;

        }

        double AudioStream::GetPitch() const {

            std::lock_guard<std::mutex> lock(mutex);

            return pitch;

        }

        void AudioStream::SetLowPass(float cutoff) {

            std::lock_guard<std::mutex> lock(mutex);

            lowPass = glm::max(0.0f, cutoff);

        }

        float AudioStream::GetLowPass() const {

            std::lock_guard<std::mutex> lock(mutex);

            return lowPass;

        }

        void AudioStream::SetReverbSend(float send) {

            std::lock_guard<std::mutex> lock(mutex);

            reverbSend = Helper::Math::Saturate(send);

        }

        float AudioStream::GetReverbSend() const {

            std::lock_guard<std::mutex> lock(mutex);

            return reverbSend;

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

        bool AudioStream::IsFinished() const {

            return finished;

        }

        bool AudioStream::GetChunk(std::vector<int16_t>& chunk) {

            if (!data.IsLoaded())
                return false;

            std::lock_guard<std::mutex> lock(mutex);

            auto channels = (int32_t)data->GetChannelCount();
            auto length = int32_t(chunk.size()) / channels;
            auto sampleCount = (double)data->data.size() / (double)channels - 1.0;

            if (volume == 0.0f) {
                // Silent one shots still play on, otherwise a sound out of hearing range would never end
                if (keepAlive && !loop) {
                    progress += pitch * double(length);
                    finished = progress >= sampleCount;
                }
                return false;
            }

            std::memset(chunk.data(), 0, chunk.size() * 2);
            auto preMultipliedChannelVolume = channelVolume;

            for (int32_t i = 0; i < channels; i++) {
                preMultipliedChannelVolume[i] *= volume;
            }

            for (int32_t i = 0; i < length; i++) {

                if (progress >= sampleCount) {
                    if (loop) {
                        progress = fmod(progress, sampleCount);
                    }
                    else {
                        finished = true;
                        break;
                    }
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

            if (lowPass > 0.0f) {
                // The coefficient of a one pole low pass at the mixing rate, two stages for 12 dB per octave
                auto rate = float(AudioManager::GetSampleRate());
                auto coefficient = 1.0f - expf(-2.0f * glm::pi<float>() * glm::min(lowPass, rate * 0.45f) / rate);
                for (int32_t i = 0; i < length; i++) {
                    for (int32_t j = 0; j < channels && j < 8; j++) {
                        auto& first = filterState[j * 2];
                        auto& second = filterState[j * 2 + 1];
                        first += (float(chunk[i * channels + j]) - first) * coefficient;
                        second += (first - second) * coefficient;
                        chunk[i * channels + j] = int16_t(second);
                    }
                }
            }

            return true;

        }

    }
    
}