#include "AudioBindings.h"

#include "audio/AudioData.h"
#include "audio/AudioStream.h"

namespace Atlas::Scripting::Bindings {

    void GenerateAudioBindings(sol::table* ns) {

        ns->new_usertype<Audio::AudioData>("AudioData",
            "GetChannelCount", &Audio::AudioData::GetChannelCount,
            "GetFrequency", &Audio::AudioData::GetFrequency,
            "GetSampleSize", &Audio::AudioData::GetSampleSize,
            "filename", &Audio::AudioData::filename
            );

        ns->new_usertype<Audio::AudioStream>("AudioStream",
            "GetDuration", &Audio::AudioStream::GetDuration,
            "SetTime", &Audio::AudioStream::SetTime,
            "GetTime", &Audio::AudioStream::GetTime,
            "SetVolume", &Audio::AudioStream::SetVolume,
            "GetVolume", &Audio::AudioStream::GetVolume,
            "SetPitch", &Audio::AudioStream::SetPitch,
            "GetPitch", &Audio::AudioStream::GetPitch,
            "Pause", &Audio::AudioStream::Pause,
            "Resume", &Audio::AudioStream::Resume,
            "IsPaused", &Audio::AudioStream::IsPaused,
            "IsValid", &Audio::AudioStream::IsValid,
            "loop", &Audio::AudioStream::loop
            );

    }

}