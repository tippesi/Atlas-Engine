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
        
        bool AudioManager::Configure(uint32_t frequency, uint8_t channels, uint32_t samples) {

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

            audioStreams.clear();

            SDL_CloseAudioDevice(audioDevice);

        }

        void AudioManager::Mute() {



        }

        void AudioManager::Unmute() {



        }

        void AudioManager::Pause() {

            SDL_PauseAudioDevice(audioDevice, 1);

        }

        void AudioManager::Resume() {

            SDL_PauseAudioDevice(audioDevice, 0);

        }

        Ref<AudioStream> AudioManager::CreateStream(ResourceHandle<AudioData> data) {

            if (!data->isValid)
                return nullptr;

            auto stream = CreateRef<AudioStream>(data);

            std::lock_guard<std::mutex> lock(mutex);
            audioStreams.push_back(stream);

            return stream;

        }

        void AudioManager::Update() {

            std::unique_lock<std::mutex> lock(mutex);

            for (size_t i = 0; i < audioStreams.size(); i++) {
                auto& ref = audioStreams[i];
                if (ref.use_count() == 1) {
                    ref.swap(audioStreams.back());
                    audioStreams.pop_back();
                    i--;
                }
            }

        }

        void AudioManager::Callback(void* userData, uint8_t* stream, int32_t length) {

            // We only use 16 bit audio internally
            length /= 2;

            std::vector<int16_t> dest(length, 0);
            std::vector<int16_t> chunk(length, 0);

            std::unique_lock<std::mutex> lock(mutex);
            auto localStreams = audioStreams;
            lock.unlock();

            // Take ownership of stream here, such that the update can run in parallel
            for (auto stream : localStreams) {

                if (!stream->IsValid() || stream->IsPaused())
                    continue;

                stream->GetChunk(chunk);

                Mix(dest, chunk);

            }

            std::memcpy(stream, dest.data(), length * 2);

        }

        void AudioManager::Mix(std::vector<int16_t>& dest, const std::vector<int16_t>& src) {

            const int16_t maxAudioValue = ((1 << (16 - 1)) - 1);
            const int16_t minAudioValue = -(1 << (16 - 1));

            for (uint64_t i = 0; i < dest.size(); i++) {

                auto sample = dest[i] + src[i];
            
                sample = sample > maxAudioValue ? maxAudioValue : sample;
                sample = sample < minAudioValue ? minAudioValue : sample;

                dest[i] = sample;
            
            }

        }

    }

}