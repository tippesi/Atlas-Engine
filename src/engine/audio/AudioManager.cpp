#include "AudioManager.h"
#include "Log.h"

#include <SDL_audio.h>
#include <algorithm>

namespace Atlas {

    namespace Audio {

        SDL_AudioSpec AudioManager::audioSpec;
        SDL_AudioDeviceID AudioManager::audioDevice;

        std::vector<Ref<AudioStream>> AudioManager::audioStreams;

        std::mutex AudioManager::mutex;

        std::atomic<float> AudioManager::masterVolume = 1.0f;
        std::atomic_bool AudioManager::muted = false;
        
        bool AudioManager::Configure(int32_t frequency, uint8_t channels, uint32_t samples) {

            std::lock_guard<std::mutex> guard(mutex);

            SDL_AudioSpec desiredSpec;

            std::memset(&desiredSpec, 0, sizeof(SDL_AudioSpec));

            desiredSpec.freq = frequency;
            desiredSpec.format = AUDIO_S16LSB;
            desiredSpec.channels = channels;
            desiredSpec.samples = samples;
            desiredSpec.callback = AudioManager::Callback;
            desiredSpec.userdata = nullptr;

            if ((audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desiredSpec, &audioSpec, 0)) != 0) {

                if (audioSpec.format != AUDIO_S16LSB)
                    return false;

                Resume();

                return true;

            }

            Log::Warning("Couldn't configure audio device with required specifications");

            return false;

        }

        void AudioManager::Shutdown() {

            Pause();

            audioStreams.clear();

            SDL_CloseAudioDevice(audioDevice);

        }

        void AudioManager::Mute() {

            muted = true;

        }

        void AudioManager::Unmute() {

            muted = false;

        }

        bool AudioManager::IsMuted() {

            return muted;

        }

        void AudioManager::SetMasterVolume(float volume) {

            masterVolume = glm::max(0.0f, volume);

        }

        float AudioManager::GetMasterVolume() {

            return masterVolume;

        }

        void AudioManager::Pause() {

            SDL_PauseAudioDevice(audioDevice, 1);

        }

        void AudioManager::Resume() {

            SDL_PauseAudioDevice(audioDevice, 0);

        }

        Ref<AudioStream> AudioManager::CreateStream(ResourceHandle<AudioData> data, float volume, bool loop) {

            auto stream = CreateRef<AudioStream>(data, volume, loop);

            std::lock_guard<std::mutex> lock(mutex);
            audioStreams.push_back(stream);

            return stream;

        }

        Ref<AudioStream> AudioManager::Play(ResourceHandle<AudioData> data, float volume, bool loop) {

            auto stream = CreateRef<AudioStream>(data, volume, loop);
            stream->keepAlive = !loop;

            std::lock_guard<std::mutex> lock(mutex);
            audioStreams.push_back(stream);

            return stream;

        }

        void AudioManager::Update() {

            std::unique_lock<std::mutex> lock(mutex);

            for (size_t i = 0; i < audioStreams.size(); i++) {
                auto& ref = audioStreams[i];
                if (ref.use_count() == 1 && (!ref->keepAlive || ref->IsFinished())) {
                    ref.swap(audioStreams.back());
                    audioStreams.pop_back();
                    i--;
                }
            }

        }

        void AudioManager::Callback(void* userData, uint8_t* streamData, int32_t length) {

            // We only use 16 bit audio internally
            length /= 2;

            std::vector<float> dest(length, 0.0f);
            std::vector<int16_t> chunk(length, 0);

            std::unique_lock<std::mutex> lock(mutex);
            auto localStreams = audioStreams;
            lock.unlock();

            // Take ownership of stream here, such that the update can run in parallel
            for (const auto& stream : localStreams) {

                if (!stream->IsValid() || stream->IsPaused())
                    continue;

                if (!stream->GetChunk(chunk))
                    continue;

                Mix(dest, chunk);

            }

            // Streams are summed in float, peaks are rounded off softly instead of clipping per stream
            auto output = reinterpret_cast<int16_t*>(streamData);
            auto gain = muted ? 0.0f : masterVolume.load() / 32768.0f;
            const float knee = 0.75f;
            for (int32_t i = 0; i < length; i++) {
                auto sample = dest[i] * gain;
                auto magnitude = glm::abs(sample);
                if (magnitude > knee)
                    sample = glm::sign(sample) * (knee + (1.0f - knee) * tanhf((magnitude - knee) / (1.0f - knee)));
                output[i] = int16_t(glm::clamp(sample, -1.0f, 1.0f) * 32767.0f);
            }

        }

        void AudioManager::Mix(std::vector<float>& dest, const std::vector<int16_t>& src) {

            for (size_t i = 0; i < dest.size(); i++)
                dest[i] += float(src[i]);

        }

    }

}