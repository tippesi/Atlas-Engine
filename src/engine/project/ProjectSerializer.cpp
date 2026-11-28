#include "ProjectSerializer.h"
#include "streaming/StreamingSettingsSerializer.h"

namespace Atlas {

    void to_json(json& j, const Project& p) {

        j = json {
            { "name", p.name },
            { "assetDirectory", p.assetDirectory },
            { "mainScript", p.mainScript },
            { "startScene", p.startScene },
            { "streaming", p.settings.streaming },
            { "rendering", { { "occlusionCulling", p.settings.occlusionCulling }, { "meshLodErrorScale", p.settings.meshLodErrorScale },
                { "upscaler", int32_t(p.settings.upscaler) } } },
            { "scenes", { { "binaryColumns", p.settings.binarySceneColumns } } }
        };

    }

    void from_json(const json& j, Project& p) {

        try_get_json(j, "name", p.name);
        try_get_json(j, "assetDirectory", p.assetDirectory);
        try_get_json(j, "mainScript", p.mainScript);
        try_get_json(j, "startScene", p.startScene);
        try_get_json(j, "engineVersion", p.engineVersion);
        try_get_json(j, "streaming", p.settings.streaming);

        // Older project files have no scene settings, the project takes them from its first scene
        p.hasSceneSettings = j.contains("rendering") && j["rendering"].contains("upscaler");
        if (p.hasSceneSettings) {
            try_get_json(j["rendering"], "occlusionCulling", p.settings.occlusionCulling);
            try_get_json(j["rendering"], "meshLodErrorScale", p.settings.meshLodErrorScale);
            p.settings.upscaler = PostProcessing::Upscaler(j["rendering"]["upscaler"].get<int32_t>());
        }
        if (j.contains("scenes"))
            try_get_json(j["scenes"], "binaryColumns", p.settings.binarySceneColumns);

    }

}
