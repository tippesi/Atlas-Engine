#include "AudioManager.h"

#include <SDL_audio.h>

namespace Atlas {

    namespace Audio {

        SDL_AudioSpec AudioManager::audioSpec;
        SDL_AudioDeviceID AudioManager::audioDevice;

        std::vector<AudioStream*> AudioManager::musicQueue;
        std::vector<AudioStream*> AudioManager::effectQueue;

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

            return false;

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

        void AudioManager::AddMusic(AudioStream *stream) {

			stream->ApplyFormat(audioSpec);

			std::lock_guard<std::mutex> lock(mutex);

			musicQueue.push_back(stream);

        }

        void AudioManager::RemoveMusic(AudioStream *stream) {

			std::lock_guard<std::mutex> lock(mutex);

        }

        void AudioManager::Callback(void* userData, uint8_t* stream, int32_t length) {

			// We only use 16 bit audio internally
			length /= 2;

			std::vector<int16_t> dest(length);

			std::memset(dest.data(), 0, length * 2);

			// Compute music first
			std::unique_lock<std::mutex> lock(mutex);

			auto queue = musicQueue;

			lock.unlock();

			for (auto stream : queue) {

				auto src = stream->GetChunk(length);

				Mix(dest, src);

			}

			// Compute audio effects
			lock.lock();



			lock.unlock();

			std::memcpy(stream, dest.data(), length * 2);

        }

		void AudioManager::Mix(std::vector<int16_t>& dest, std::vector<int16_t>& src) {

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