#include "AudioBindings.h"

#include "audio/AudioData.h"
#include "audio/AudioStream.h"
#include "audio/AudioManager.h"

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
            "IsFinished", &Audio::AudioStream::IsFinished,
            "SetPan", &Audio::AudioStream::SetPan,
            "loop", &Audio::AudioStream::loop
            );

        // Audio without entities: Play(audio, volume, loop) returns the stream, see AudioManager::Play
        auto audio = ns->create_named("AudioManager");
        audio.set_function("Play", [](ResourceHandle<Audio::AudioData> data, sol::optional<float> volume,
            sol::optional<bool> loop) {
            return Audio::AudioManager::Play(data, volume.value_or(1.0f), loop.value_or(false));
        });
        audio.set_function("SetMasterVolume", &Audio::AudioManager::SetMasterVolume);
        audio.set_function("GetMasterVolume", &Audio::AudioManager::GetMasterVolume);
        audio.set_function("Mute", &Audio::AudioManager::Mute);
        audio.set_function("Unmute", &Audio::AudioManager::Unmute);
        audio.set_function("IsMuted", &Audio::AudioManager::IsMuted);

    }

}