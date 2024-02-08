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

            explicit AudioStream(ResourceHandle<AudioData> data, float volume, bool loop = false);

            AudioStream& operator=(const AudioStream& that) = delete;

            void ChangeData(ResourceHandle<AudioData> data);

            double GetDuration();

            void SetTime(double time);

            double GetTime() const;

            void SetVolume(float volume);

            float GetVolume() const;

            void SetChannelVolume(Channel channel, float volume);

            float GetChannelVolume(Channel channel) const;

            void SetPitch(double pitch);

            double GetPitch() const;

            void Pause();

            void Resume();

            bool IsPaused() const;

            bool IsValid() const;

            bool GetChunk(std::vector<int16_t>& chunk);

            bool loop = false;

            ResourceHandle<AudioData> data;

        private:
            double progress = 0.0;

            float volume = 1.0f;
            double pitch = 1.0f;

            bool pause = false;

            std::array<float, 8> channelVolume;

            mutable std::mutex mutex;

        };

    }

}