#ifndef AE_AUDIOMANAGER_H
#define AE_AUDIOMANAGER_H

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

            static void Mute();

            static void Unmute();

            static void Pause();

            static void Resume();

            static void AddMusic(AudioStream* stream);

            static void RemoveMusic(AudioStream* stream);

        private:
            static void Callback(void* userData, uint8_t* stream, int32_t length);

			static void Mix(std::vector<int16_t>& dest, std::vector<int16_t>& src);

            static SDL_AudioSpec audioSpec;
            static SDL_AudioDeviceID audioDevice;

            static std::vector<AudioStream*> musicQueue;
            static std::vector<AudioStream*> effectQueue;

			static std::mutex mutex;

        };

    }

}


#endif
