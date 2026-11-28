#include "NavigationSerializer.h"

namespace Atlas::Navigation {

    void to_json(json& j, const NavigationSettings& p) {
        j = json {
            {"agentRadius", p.agentRadius},
            {"agentHeight", p.agentHeight},
            {"agentMaxClimb", p.agentMaxClimb},
            {"agentMaxSlope", p.agentMaxSlope},
            {"cellSize", p.cellSize},
            {"cellHeight", p.cellHeight},
            {"tileSize", p.tileSize}
        };
    }

    void from_json(const json& j, NavigationSettings& p) {
        try_get_json(j, "agentRadius", p.agentRadius);
        try_get_json(j, "agentHeight", p.agentHeight);
        try_get_json(j, "agentMaxClimb", p.agentMaxClimb);
        try_get_json(j, "agentMaxSlope", p.agentMaxSlope);
        try_get_json(j, "cellSize", p.cellSize);
        try_get_json(j, "cellHeight", p.cellHeight);
        try_get_json(j, "tileSize", p.tileSize);
    }

    void to_json(json& j, const Navigation& p) {
        j = json {
            {"enable", p.enable},
            {"radius", p.radius},
            {"settings", p.settings},
            {"maxTileStartsPerUpdate", p.maxTileStartsPerUpdate},
            {"maxConcurrentBuilds", p.maxConcurrentBuilds},
            {"buildPriority", int32_t(p.buildPriority)}
        };
    }

    void from_json(const json& j, Navigation& p) {
        try_get_json(j, "enable", p.enable);
        try_get_json(j, "radius", p.radius);
        try_get_json(j, "settings", p.settings);
        try_get_json(j, "maxTileStartsPerUpdate", p.maxTileStartsPerUpdate);
        try_get_json(j, "maxConcurrentBuilds", p.maxConcurrentBuilds);
        if (j.contains("buildPriority"))
            p.buildPriority = JobPriority(j["buildPriority"].get<int32_t>());
    }

}
