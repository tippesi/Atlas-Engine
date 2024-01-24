#pragma once

#include "../System.h"

#include <SDL_audio.h>

#include <string>
#include <vector>
#include <mutex>

namespace Atlas {

    namespace Audio {

        class AudioData {

        public:
            AudioData();

            explicit AudioData(const std::string& filename);

            bool ApplyFormat(const SDL_AudioSpec& formatSpec);

            bool Convert(uint32_t frequency, uint8_t channels, uint32_t format);

            uint8_t GetChannelCount();

            int32_t GetSampleSize();

            int32_t GetFrequency();

            std::vector<int16_t> data;

            const std::string filename;

        private:
            SDL_AudioSpec spec;

            bool isValid;

            friend class AudioManager;
            friend class AudioStream;

        };

    }

}