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

            // -1 is fully left, 1 fully right
            void SetPan(float pan);

            /**
             * Sets volume and panning for a source at a location heard by a listener, returns whether
             * the source is audible.
             */
            bool Spatialize(vec3 source, vec3 listener, vec3 listenerRight, float volume, float falloffFactor,
                float falloffPower, float cutoff);

            void SetPitch(double pitch);

            double GetPitch() const;

            // Frequencies above the cutoff (Hz) are damped, e.g. under water or behind a wall, 0 turns the filter off
            void SetLowPass(float cutoff);

            float GetLowPass() const;

            // How much of the stream goes into the reverb of the audio manager, 0 to 1
            void SetReverbSend(float send);

            float GetReverbSend() const;

            void Pause();

            void Resume();

            bool IsPaused() const;

            bool IsValid() const;

            // A stream without loop that played to its end
            bool IsFinished() const;

            bool GetChunk(std::vector<int16_t>& chunk);

            bool loop = false;

            ResourceHandle<AudioData> data;

        private:
            double progress = 0.0;

            float volume = 1.0f;
            double pitch = 1.0f;

            float lowPass = 0.0f;
            float reverbSend = 0.0f;
            // Two one pole stages per channel
            std::array<float, 16> filterState = {};

            bool pause = false;
            std::atomic_bool finished = false;
            // Played to the end by the audio manager, even when nobody holds a reference anymore
            bool keepAlive = false;

            std::array<float, 8> channelVolume;

            mutable std::mutex mutex;

            friend class AudioManager;

        };

    }

}