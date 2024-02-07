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
        p = AO(j["sampleCount"].get<int32_t>());
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
        p = EnvironmentProbe(j["resolution"].get<int32_t>(),
            j["position"].get<vec3>());
    }

    void to_json(json& j, const Atmosphere& p) {
        j = json {
            {"height", p.height},
            {"probeResolution", p.probe->resolution}
        };
    }

    void from_json(const json& j, Atmosphere& p) {
        p = Atmosphere(j["height"].get<float>(),
            j["probeResolution"].get<int32_t>());
    }

    void to_json(json& j, const Fog& p) {
        j = json {
            {"enable", p.enable},
            {"scatteringFactor", p.scatteringFactor},
            {"extinctionFactor", p.extinctionFactor},
            {"extinctionCoefficients", p.extinctionCoefficients},
            {"ambientFactor", p.ambientFactor},
            {"density", p.density},
            {"height", p.height},
            {"heightFalloff", p.heightFalloff},
            {"scatteringAnisotropy", p.scatteringAnisotropy},
            {"rayMarching", p.rayMarching},
            {"rayMarchStepCount", p.rayMarchStepCount},
            {"volumetricIntensity", p.volumetricIntensity},
        };
    }

    void from_json(const json& j, Fog& p) {
        j.at("enable").get_to(p.enable);
        j.at("scatteringFactor").get_to(p.scatteringFactor);
        j.at("extinctionFactor").get_to(p.extinctionFactor);
        j.at("extinctionCoefficients").get_to(p.extinctionCoefficients);
        j.at("ambientFactor").get_to(p.ambientFactor);
        j.at("density").get_to(p.density);
        j.at("height").get_to(p.height);
        j.at("heightFalloff").get_to(p.heightFalloff);
        j.at("scatteringAnisotropy").get_to(p.scatteringAnisotropy);
        j.at("rayMarching").get_to(p.rayMarching);
        j.at("rayMarchStepCount").get_to(p.rayMarchStepCount);
        j.at("volumetricIntensity").get_to(p.volumetricIntensity);
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
        p = IrradianceVolume(j["aabb"].get<Volume::AABB>(),
            j["probeCount"].get<ivec3>(), j["lowerResMoments"].get<bool>());
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

    void to_json(json& j, const SSGI& p) {
        j = json {
            {"enable", p.enable},
            {"enableAo", p.enableAo},
            {"radius", p.radius},
            {"rayCount", p.rayCount},
            {"sampleCount", p.sampleCount},
            {"irradianceLimit", p.irradianceLimit},
            {"aoStrength", p.aoStrength},
            {"rt", p.rt},
            {"opacityCheck", p.opacityCheck},
        };
    }

    void from_json(const json& j, SSGI& p) {
        j.at("enable").get_to(p.enable);
        j.at("enableAo").get_to(p.enableAo);
        j.at("radius").get_to(p.radius);
        j.at("rayCount").get_to(p.rayCount);
        j.at("sampleCount").get_to(p.sampleCount);
        j.at("irradianceLimit").get_to(p.irradianceLimit);
        j.at("aoStrength").get_to(p.aoStrength);
        j.at("rt").get_to(p.rt);
        j.at("opacityCheck").get_to(p.opacityCheck);
    }

    void to_json(json& j, const SSS& p) {
        j = json {
            {"sampleCount", p.sampleCount},
            {"maxLength", p.maxLength},
            {"thickness", p.thickness},
            {"enable", p.enable},
        };
    }

    void from_json(const json& j, SSS& p) {
        j.at("sampleCount").get_to(p.sampleCount);
        j.at("maxLength").get_to(p.maxLength);
        j.at("thickness").get_to(p.thickness);
        j.at("enable").get_to(p.enable);
    }

    void to_json(json& j, const VolumetricClouds::Scattering& p) {
        j = json {
            {"extinctionFactor", p.extinctionFactor},
            {"scatteringFactor", p.scatteringFactor},
            {"extinctionCoefficients", p.extinctionCoefficients},
            {"eccentricityFirstPhase", p.eccentricityFirstPhase},
            {"eccentricitySecondPhase", p.eccentricitySecondPhase},
            {"phaseAlpha", p.phaseAlpha},
        };
    }

    void from_json(const json& j, VolumetricClouds::Scattering& p) {
        j.at("extinctionFactor").get_to(p.extinctionFactor);
        j.at("scatteringFactor").get_to(p.scatteringFactor);
        j.at("extinctionCoefficients").get_to(p.extinctionCoefficients);
        j.at("eccentricityFirstPhase").get_to(p.eccentricityFirstPhase);
        j.at("eccentricitySecondPhase").get_to(p.eccentricitySecondPhase);
        j.at("phaseAlpha").get_to(p.phaseAlpha);
    }

    void to_json(json& j, const VolumetricClouds& p) {
        j = json {
            {"coverageResolution", p.coverageTexture.width},
            {"shapeResolution", p.shapeTexture.width},
            {"detailResolution", p.detailTexture.width},
            {"shadowResolution", p.shadowTexture.width},
            {"sampleCount", p.sampleCount},
            {"occlusionSampleCount", p.occlusionSampleCount},
            {"shadowSampleFraction", p.shadowSampleFraction},
            {"minHeight", p.minHeight},
            {"maxHeight", p.maxHeight},
            {"distanceLimit", p.distanceLimit},
            {"coverageScale", p.coverageScale},
            {"shapeScale", p.shapeScale},
            {"detailScale", p.detailScale},
            {"coverageSpeed", p.coverageSpeed},
            {"shapeSpeed", p.shapeSpeed},
            {"detailSpeed", p.detailSpeed},
            {"detailStrength", p.detailStrength},
            {"densityMultiplier", p.densityMultiplier},
            {"heightStretch", p.heightStretch},
            {"darkEdgeFocus", p.darkEdgeFocus},
            {"darkEdgeAmbient", p.darkEdgeAmbient},
            {"scattering", p.scattering},
            {"enable", p.enable},
            {"castShadow", p.castShadow},
            {"stochasticOcclusionSampling", p.stochasticOcclusionSampling},
        };
    }

    void from_json(const json& j, VolumetricClouds& p) {
        p = VolumetricClouds(j["coverageResolution"].get<int32_t>(), j["shapeResolution"].get<int32_t>(),
            j["detailResolution"].get<int32_t>(), j["shadowResolution"].get<int32_t>());

        j.at("sampleCount").get_to(p.sampleCount);
        j.at("occlusionSampleCount").get_to(p.occlusionSampleCount);
        j.at("shadowSampleFraction").get_to(p.shadowSampleFraction);
        j.at("minHeight").get_to(p.minHeight);
        j.at("maxHeight").get_to(p.maxHeight);
        j.at("distanceLimit").get_to(p.distanceLimit);
        j.at("coverageScale").get_to(p.coverageScale);
        j.at("shapeScale").get_to(p.shapeScale);
        j.at("detailScale").get_to(p.detailScale);
        j.at("coverageSpeed").get_to(p.coverageSpeed);
        j.at("shapeSpeed").get_to(p.shapeSpeed);
        j.at("detailSpeed").get_to(p.detailSpeed);
        j.at("detailStrength").get_to(p.detailStrength);
        j.at("densityMultiplier").get_to(p.densityMultiplier);
        j.at("heightStretch").get_to(p.heightStretch);
        j.at("darkEdgeFocus").get_to(p.darkEdgeFocus);
        j.at("darkEdgeAmbient").get_to(p.darkEdgeAmbient);
        j.at("scattering").get_to(p.scattering);
        j.at("enable").get_to(p.enable);
        j.at("castShadow").get_to(p.castShadow);
        j.at("stochasticOcclusionSampling").get_to(p.stochasticOcclusionSampling);
    }

    void to_json(json& j, const Sky& p) {
        j = json {
            {"planetCenter", p.planetCenter},
            {"planetRadius", p.planetRadius}
        };

        if (p.atmosphere)
            j["atmosphere"] = *p.atmosphere;
        if (p.clouds)
            j["clouds"] = *p.clouds;
        if (p.probe)
            j["probe"] = *p.probe;
    }

    void from_json(const json& j, Sky& p) {
        j.at("planetCenter").get_to(p.planetCenter);
        j.at("planetRadius").get_to(p.planetRadius);

        if (j.contains("atmosphere")) {
            p.atmosphere = CreateRef<Atmosphere>();
            *p.atmosphere = j["atmosphere"];
        }
        if (j.contains("clouds")) {
            p.clouds = CreateRef<VolumetricClouds>();
            *p.clouds = j["clouds"];
        }
        if (j.contains("probe")) {
            p.probe = CreateRef<EnvironmentProbe>();
            *p.probe = j["probe"];
        }
    }

}