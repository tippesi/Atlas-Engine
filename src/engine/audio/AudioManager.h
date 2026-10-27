#pragma once

#include "../System.h"

#include "AudioStream.h"

#include <SDL_audio.h>

#include <atomic>
#include <vector>
#include <mutex>

namespace Atlas {

    namespace Audio {

        class AudioManager {

        public:
            static bool Configure(int32_t frequency, uint8_t channels, uint32_t samples);

            static void Shutdown();

            static void Mute();

            static void Unmute();

            static void Pause();

            static void Resume();

            static Ref<AudioStream> CreateStream(ResourceHandle<AudioData> data, float volume = 1.0f, bool loop = false);

            /**
             * Plays audio without an entity. A sound without loop plays to its end even if the returned
             * stream isn't kept, a looping one stops once the last reference to the stream is dropped.
             */
            static Ref<AudioStream> Play(ResourceHandle<AudioData> data, float volume = 1.0f, bool loop = false);

            static void SetMasterVolume(float volume);

            static float GetMasterVolume();

            static bool IsMuted();

            static void Update();

        private:
            static void Callback(void* userData, uint8_t* stream, int32_t length);

            static void Mix(std::vector<float>& dest, const std::vector<int16_t>& src);

            static SDL_AudioSpec audioSpec;
            static SDL_AudioDeviceID audioDevice;

            static std::vector<Ref<AudioStream>> audioStreams;

            static std::mutex mutex;

            static std::atomic<float> masterVolume;
            static std::atomic_bool muted;

            friend AudioData;

        };

    }

}