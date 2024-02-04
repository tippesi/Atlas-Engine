#pragma once

#include "common/SerializationHelper.h"

#include "Shadow.h"

namespace Atlas::Lighting {

    void to_json(json& j, const AO& p) {
        j = json {
            {"sampleCount", p.sampleCount},
            {"radius", p.radius},
            {"strength", p.strength},
            {"enable", p.enable},
            {"rt", p.rt},
            {"opacityCheck", p.opacityCheck},
        };
    }

    void from_json(const json& j, AO& p) {
        p = AO(j["sampleCount"]);
        j.at("radius").get_to(p.radius);
        j.at("strength").get_to(p.strength);
        j.at("enable").get_to(p.enable);
        j.at("rt").get_to(p.rt);
        j.at("opacityCheck").get_to(p.opacityCheck);
    }

    void to_json(json& j, const EnvironmentProbe& p) {
        j = json {
            {"resolution", p.resolution},
            {"position", p.GetPosition()}
        };
    }

    void from_json(const json& j, EnvironmentProbe& p) {
        p = EnvironmentProbe(j["resolution"], j["position"]);
    }

    void to_json(json& j, const Atmosphere& p) {
        j = json {
            {"height", p.height},
            {"probeResolution", p.probe->resolution}
        };
    }

    void from_json(const json& j, Atmosphere& p) {
        p = Atmosphere(j["height"], j["probeResolution"]);
    }

    void to_json(json& j, const Fog& p) {
        j = json {
            {"enable", p.enable},
            {"color", p.color},
            {"density", p.density},
            {"height", p.height},
            {"heightFalloff", p.heightFalloff},
            {"scatteringAnisotropy", p.scatteringAnisotropy},
            {"rayMarching", p.rayMarching},
            {"rayMarchStepCount", p.rayMarchStepCount},
        };
    }

    void from_json(const json& j, Fog& p) {
        j.at("enable").get_to(p.enable);
        j.at("color").get_to(p.color);
        j.at("density").get_to(p.density);
        j.at("height").get_to(p.height);
        j.at("heightFalloff").get_to(p.heightFalloff);
        j.at("scatteringAnisotropy").get_to(p.scatteringAnisotropy);
        j.at("rayMarching").get_to(p.rayMarching);
        j.at("rayMarchStepCount").get_to(p.rayMarchStepCount);
    }

    void to_json(json& j, const IrradianceVolume& p) {
        j = json {
            {"aabb", p.aabb},
            {"probeCount", p.probeCount},
            {"lowerResMoments", p.lowerResMoments},
            {"enable", p.enable},
            {"rayCount", p.rayCount},
            {"rayCountInactive", p.rayCountInactive},
            {"hysteresis", p.hysteresis},
            {"bias", p.bias},
            {"sharpness", p.sharpness},
            {"gamma", p.gamma},
            {"strength", p.strength},
            {"sampleEmissives", p.sampleEmissives},
            {"optimizeProbes", p.optimizeProbes},
            {"useShadowMap", p.useShadowMap},
            {"opacityCheck", p.opacityCheck},
        };
    }

    void from_json(const json& j, IrradianceVolume& p) {
        p = IrradianceVolume(j["aabb"], j["probeCount"], j["lowerResMoments"]);
        j.at("enable").get_to(p.enable);
        j.at("rayCount").get_to(p.rayCount);
        j.at("rayCountInactive").get_to(p.rayCountInactive);
        j.at("hysteresis").get_to(p.hysteresis);
        j.at("bias").get_to(p.bias);
        j.at("sharpness").get_to(p.sharpness);
        j.at("gamma").get_to(p.gamma);
        j.at("strength").get_to(p.strength);
        j.at("sampleEmissives").get_to(p.sampleEmissives);
        j.at("optimizeProbes").get_to(p.optimizeProbes);
        j.at("useShadowMap").get_to(p.useShadowMap);
        j.at("opacityCheck").get_to(p.opacityCheck);
    }

    void to_json(json& j, const Reflection& p) {
        j = json {
            {"textureLevel", p.textureLevel},
            {"radianceLimit", p.radianceLimit},
            {"bias", p.bias},
            {"spatialFilterStrength", p.spatialFilterStrength},
            {"temporalWeight", p.temporalWeight},
            {"historyClipMax", p.historyClipMax},
            {"currentClipFactor", p.currentClipFactor},
            {"enable", p.enable},
            {"rt", p.rt},
            {"gi", p.gi},
            {"useShadowMap", p.useShadowMap},
            {"opacityCheck", p.opacityCheck},
        };
    }

    void from_json(const json& j, Reflection& p) {
        j.at("textureLevel").get_to(p.textureLevel);
        j.at("radianceLimit").get_to(p.radianceLimit);
        j.at("bias").get_to(p.bias);
        j.at("spatialFilterStrength").get_to(p.spatialFilterStrength);
        j.at("temporalWeight").get_to(p.temporalWeight);
        j.at("historyClipMax").get_to(p.historyClipMax);
        j.at("currentClipFactor").get_to(p.currentClipFactor);
        j.at("enable").get_to(p.enable);
        j.at("rt").get_to(p.rt);
        j.at("gi").get_to(p.gi);
        j.at("useShadowMap").get_to(p.useShadowMap);
        j.at("opacityCheck").get_to(p.opacityCheck);
    }

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