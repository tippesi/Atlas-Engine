#pragma once

#include "common/SerializationHelper.h"

#include "Shadow.h"

namespace Atlas::Lighting {

    void to_json(json& j, const ShadowView& p) {
        j = json {
            {"nearDistance", p.nearDistance},
            {"farDistance", p.farDistance},
            {"viewMatrix", p.viewMatrix},
            {"projectionMatrix", p.projectionMatrix},
            {"frustumMatrix", p.frustumMatrix},
            {"terrainFrustumMatrix", p.terrainFrustumMatrix},
        };
    }

    void from_json(const json& j, ShadowView& p) {
        j.at("nearDistance").get_to(p.nearDistance);
        j.at("farDistance").get_to(p.farDistance);
        j.at("viewMatrix").get_to(p.viewMatrix);
        j.at("projectionMatrix").get_to(p.projectionMatrix);
        j.at("frustumMatrix").get_to(p.frustumMatrix);
        j.at("terrainFrustumMatrix").get_to(p.terrainFrustumMatrix);
    }

    void to_json(json& j, const Shadow& p) {
        j = json {
            {"center", p.center},
            {"distance", p.distance},
            {"longRangeDistance", p.longRangeDistance},
            {"bias", p.bias},
            {"splitCorrection", p.splitCorrection},
            {"cascadeBlendDistance", p.cascadeBlendDistance},
            {"resolution", p.resolution},
            {"views", p.views},
            {"viewCount", p.viewCount},
            {"isCascaded", p.isCascaded},
            {"useCubemap", p.useCubemap},
            {"allowDynamicActors", p.allowDynamicActors},
            {"allowTerrain", p.allowTerrain},
            {"longRange", p.longRange}
        };
    }

    void from_json(const json& j, Shadow& p) {
        j.at("center").get_to(p.center);
        j.at("distance").get_to(p.distance);
        j.at("longRangeDistance").get_to(p.longRangeDistance);
        j.at("bias").get_to(p.bias);
        j.at("splitCorrection").get_to(p.splitCorrection);
        j.at("cascadeBlendDistance").get_to(p.cascadeBlendDistance);
        j.at("resolution").get_to(p.resolution);
        j.at("views").get_to(p.views);
        j.at("viewCount").get_to(p.viewCount);
        j.at("isCascaded").get_to(p.isCascaded);
        j.at("useCubemap").get_to(p.useCubemap);
        j.at("allowDynamicActors").get_to(p.allowDynamicActors);
        j.at("allowTerrain").get_to(p.allowTerrain);
        j.at("longRange").get_to(p.longRange);

        p.SetResolution(p.resolution);
    }

}