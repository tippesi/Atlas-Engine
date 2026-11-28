#include "StreamingSettingsSerializer.h"

namespace Atlas::Streaming {

    static json LimitsToJson(const StreamingSettings::Limits& p) {

        return json {
            { "enable", p.enable },
            { "budget", p.budget },
            { "reductionDelay", p.reductionDelay },
            { "maxPendingLoads", p.maxPendingLoads }
        };

    }

    static void LimitsFromJson(const json& j, StreamingSettings::Limits& p) {

        try_get_json(j, "enable", p.enable);
        try_get_json(j, "budget", p.budget);
        try_get_json(j, "reductionDelay", p.reductionDelay);
        try_get_json(j, "maxPendingLoads", p.maxPendingLoads);

    }

    static json LevelsToJson(const StreamingSettings::Levels& p) {

        auto j = LimitsToJson(p);
        j["minResolution"] = p.minResolution;
        j["resolutionScale"] = p.resolutionScale;
        return j;

    }

    static void LevelsFromJson(const json& j, StreamingSettings::Levels& p) {

        LimitsFromJson(j, p);
        try_get_json(j, "minResolution", p.minResolution);
        try_get_json(j, "resolutionScale", p.resolutionScale);

    }

    void to_json(json& j, const StreamingSettings& p) {

        auto meshes = LimitsToJson(p.meshes);
        meshes["distanceMargin"] = p.meshes.distanceMargin;
        meshes["rayTracingDistance"] = p.meshes.rayTracingDistance;

        j = json {
            { "textures", LevelsToJson(p.textures) },
            { "impostors", LevelsToJson(p.impostors) },
            { "meshes", meshes }
        };

    }

    void from_json(const json& j, StreamingSettings& p) {

        if (j.contains("textures"))
            LevelsFromJson(j["textures"], p.textures);
        if (j.contains("impostors"))
            LevelsFromJson(j["impostors"], p.impostors);
        if (j.contains("meshes")) {
            LimitsFromJson(j["meshes"], p.meshes);
            try_get_json(j["meshes"], "distanceMargin", p.meshes.distanceMargin);
            try_get_json(j["meshes"], "rayTracingDistance", p.meshes.rayTracingDistance);
        }

    }

}
