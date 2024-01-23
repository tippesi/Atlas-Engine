#pragma once

#include "../System.h"

#include "AudioData.h"
#include "../resource/Resource.h"

#include <vector>
#include <array>
#include <atomic>

namespace Atlas {

    namespace Audio {

        enum Channel {
            Left = 0,
            Right = 1
        };

        class AudioStream {

        public:
            AudioStream()  { channelVolume.fill(1.0f); }

            explicit AudioStream(ResourceHandle<AudioData> data) : data(data) { channelVolume.fill(1.0f); }

            AudioStream& operator=(const AudioStream& that) = delete;

            double GetDuration();

            void SetTime(double time);

            double GetTime();

            void SetVolume(float volume);

            float GetVolume();

            void SetChannelVolume(Channel channel, float volume);

            float GetChannelVolume(Channel channel);

            void SetPitch(double pitch);

            double GetPitch();

            void Pause();

            void Resume();

            bool IsPaused();

            bool IsValid();

            bool GetChunk(std::vector<int16_t>& chunk);

            bool loop = false;

        private:
            double progress = 0.0;

            float volume = 1.0f;
            double pitch = 1.0f;

            bool pause = false;

            std::array<float, 8> channelVolume;

            ResourceHandle<AudioData> data;

            mutable std::mutex mutex;

        };

    }

}