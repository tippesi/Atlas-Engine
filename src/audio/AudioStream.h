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

			AudioStream& operator=(const AudioStream& that);

			double GetDuration();

			void SetTime(double time);

			double GetTime();

			void SetVolume(float volume);

			float GetVolume();

			void SetPitch(double pitch);

			double GetPitch();

			void ApplyFormat(SDL_AudioSpec& spec);

			virtual std::vector<int16_t> GetChunk(int32_t length);

			bool loop = false;

        private:
            double progress = 0.0;

			float volume = 1.0f;
			double pitch = 1.0f;

			AudioData* data;

			mutable std::mutex mutex;

        };

    }

}

#endif