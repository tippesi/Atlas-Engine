#pragma once

#include "../System.h"

#include "AudioStream.h"

#include <SDL_audio.h>

#include <vector>
#include <mutex>

namespace Atlas {

    namespace Audio {

        class AudioManager {

        public:
            static bool Configure(uint32_t frequency, uint8_t channels, uint32_t samples);

            static void Shutdown();

            static void Mute();

            static void Unmute();

            static void Pause();

            static void Resume();

            static Ref<AudioStream> CreateStream(ResourceHandle<AudioData> data);

            static void Update();

        private:
            static void Callback(void* userData, uint8_t* stream, int32_t length);

            static void Mix(std::vector<int16_t>& dest, const std::vector<int16_t>& src);

            static SDL_AudioSpec audioSpec;
            static SDL_AudioDeviceID audioDevice;

            static std::vector<Ref<AudioStream>> audioStreams;

            static std::mutex mutex;

            friend AudioData;

        };

    }

}