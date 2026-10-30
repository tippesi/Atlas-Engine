#include "AudioManager.h"
#include "Log.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_hints.h>
#include <string>
#include <algorithm>

namespace Atlas {

    namespace Audio {

        SDL_AudioSpec AudioManager::audioSpec;
        SDL_AudioStream* AudioManager::deviceStream = nullptr;

        std::vector<Ref<AudioStream>> AudioManager::audioStreams;
        std::vector<Ref<AudioStream>> AudioManager::mixStreams;
        std::vector<float> AudioManager::mixBuffer;
        std::vector<int16_t> AudioManager::chunkBuffer;

        std::mutex AudioManager::mutex;

        std::atomic<float> AudioManager::masterVolume = 1.0f;
        std::atomic_bool AudioManager::muted = false;
        
        bool AudioManager::Configure(int32_t frequency, uint8_t channels, uint32_t samples) {

            std::lock_guard<std::mutex> guard(mutex);

            audioSpec.freq = frequency;
            audioSpec.format = SDL_AUDIO_S16LE;
            audioSpec.channels = channels;

            SDL_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, std::to_string(samples).c_str());

            // The stream converts to the device format, so the audio spec is always the requested one
            deviceStream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audioSpec,
                AudioManager::Callback, nullptr);
            if (deviceStream) {

                Resume();

                return true;

            }

            Log::Warning("Couldn't configure audio device with required specifications");

            return false;

        }

        void AudioManager::Shutdown() {

            Pause();

            audioStreams.clear();

            SDL_DestroyAudioStream(deviceStream);
            deviceStream = nullptr;

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

            SDL_PauseAudioStreamDevice(deviceStream);

        }

        void AudioManager::Resume() {

            SDL_ResumeAudioStreamDevice(deviceStream);

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

        void AudioManager::Callback(void* userData, SDL_AudioStream* audioStream, int32_t additionalAmount, int32_t totalAmount) {

            // We only use 16 bit audio internally
            auto length = additionalAmount / (2 * audioSpec.channels) * audioSpec.channels;
            if (length <= 0)
                return;

            mixBuffer.assign(length, 0.0f);
            chunkBuffer.assign(length, 0);

            std::unique_lock<std::mutex> lock(mutex);
            mixStreams = audioStreams;
            lock.unlock();

            // Take ownership of stream here, such that the update can run in parallel
            for (const auto& stream : mixStreams) {

                if (!stream->IsValid() || stream->IsPaused())
                    continue;

                if (!stream->GetChunk(chunkBuffer))
                    continue;

                Mix(mixBuffer, chunkBuffer);

            }

            // Release the streams here instead of in the next callback
            mixStreams.clear();

            // Streams are summed in float, peaks are rounded off softly instead of clipping per stream
            auto& output = chunkBuffer;
            auto gain = muted ? 0.0f : masterVolume.load() / 32768.0f;
            const float knee = 0.75f;
            for (int32_t i = 0; i < length; i++) {
                auto sample = mixBuffer[i] * gain;
                auto magnitude = glm::abs(sample);
                if (magnitude > knee)
                    sample = glm::sign(sample) * (knee + (1.0f - knee) * tanhf((magnitude - knee) / (1.0f - knee)));
                output[i] = int16_t(glm::clamp(sample, -1.0f, 1.0f) * 32767.0f);
            }

            SDL_PutAudioStreamData(audioStream, output.data(), length * int32_t(sizeof(int16_t)));

        }

        void AudioManager::Mix(std::vector<float>& dest, const std::vector<int16_t>& src) {

            for (size_t i = 0; i < dest.size(); i++)
                dest[i] += float(src[i]);

        }

    }

}