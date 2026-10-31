#pragma once

#include "../System.h"

#include "AudioStream.h"

#include <SDL3/SDL_audio.h>

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
            static void Callback(void* userData, SDL_AudioStream* audioStream, int32_t additionalAmount, int32_t totalAmount);

            static void Mix(std::vector<float>& dest, const std::vector<int16_t>& src);

            static SDL_AudioSpec audioSpec;
            static SDL_AudioStream* deviceStream;

            static std::vector<Ref<AudioStream>> audioStreams;

            // Only used by the audio callback, kept to not allocate on the audio thread
            static std::vector<Ref<AudioStream>> mixStreams;
            static std::vector<float> mixBuffer;
            static std::vector<int16_t> chunkBuffer;

            static std::mutex mutex;

            static std::atomic<float> masterVolume;
            static std::atomic_bool muted;

            friend AudioData;

        };

    }

}