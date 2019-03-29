#ifndef AE_AUDIOSTREAM_H
#define AE_AUDIOSTREAM_H

#include "../System.h"

#include "AudioData.h"

#include <vector>

namespace Atlas {

    namespace Audio {

        class AudioStream {

        public:
			AudioStream() : data(nullptr) {}

            AudioStream(AudioData* data) : data(data) {}

			double GetDuration();

			double GetTime();

			void SetTime(double time);

            std::vector<int16_t> GetChunk(int32_t length);

			void ApplyFormat(SDL_AudioSpec& spec);

			float volume = 1.0f;
			double pitch = 1.0f;

			bool loop = false;

			float leftChannelVolume = 1.0f;
			float rightChannelVolume = 1.0f;

        private:
            double progress = 0.0;			

			AudioData* data;

        };

    }

}

#endif