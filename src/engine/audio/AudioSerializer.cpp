#include "AudioSerializer.h"

namespace Atlas::Audio {

    void to_json(json& j, const AudioStream& p) {

        j = json{
            {"time", p.GetTime()},
            {"pitch", p.GetPitch()},
            {"volume", p.GetVolume()},
            {"loop", p.loop},
        };

        if (p.data.IsValid())
            j["resourcePath"] = p.data.GetResource()->path;

    }

    void from_json(const json& j, AudioStream& p) {

        double time, pitch;
        float volume;

        j.at("time").get_to(time);
        j.at("pitch").get_to(pitch);
        j.at("volume").get_to(volume);
        j.at("loop").get_to(p.loop);

        p.SetTime(time);
        p.SetPitch(pitch);
        p.SetVolume(volume);

        if (j.contains("resourcePath")) {
            std::string resourcePath = j["resourcePath"];
            auto handle = ResourceManager<Audio::AudioData>::GetOrLoadResourceAsync(resourcePath);
            p.ChangeData(handle);
        }

    }

}