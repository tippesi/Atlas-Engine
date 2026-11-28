#include "LightingBindings.h"

#include "lighting/Shadow.h"
#include "lighting/SSGI.h"
#include "lighting/EnvironmentProbe.h"
#include "lighting/SSS.h"
#include "lighting/IrradianceVolume.h"
#include "lighting/Atmosphere.h"
#include "lighting/Reflection.h"
#include "lighting/RTGI.h"
#include "lighting/Fog.h"
#include "lighting/VolumetricClouds.h"
#include "lighting/Sky.h"
#include "resource/ResourceManager.h"
#include "texture/Cubemap.h"

namespace Atlas::Scripting::Bindings {

    void GenerateLightingBindings(Binder* ns) {

        ns->new_usertype<Lighting::IrradianceVolume>("IrradianceVolume",
            sol::no_constructor,
            "enable", &Lighting::IrradianceVolume::enable,
            "storageScale", &Lighting::IrradianceVolume::storageScale,
            "ClearProbes", &Lighting::IrradianceVolume::ClearProbes
        );

        ns->new_usertype<Lighting::VolumetricClouds::Scattering>("VolumetricCloudsScattering",
            "extinctionFactor", &Lighting::VolumetricClouds::Scattering::extinctionFactor,
            "scatteringFactor", &Lighting::VolumetricClouds::Scattering::scatteringFactor,
            "extinctionCoefficients", &Lighting::VolumetricClouds::Scattering::extinctionCoefficients,
            "eccentricityFirstPhase", &Lighting::VolumetricClouds::Scattering::eccentricityFirstPhase,
            "eccentricitySecondPhase", &Lighting::VolumetricClouds::Scattering::eccentricitySecondPhase,
            "phaseAlpha", &Lighting::VolumetricClouds::Scattering::phaseAlpha
            );

        // The features of the sky are created as references, e.g. scene.sky.clouds = Atlas.VolumetricClouds.new()
        ns->new_usertype<Lighting::VolumetricClouds>("VolumetricClouds",
            "new", sol::factories([]() { return CreateRef<Lighting::VolumetricClouds>(); }),
            "coverageTexture", &Lighting::VolumetricClouds::coverageTexture,
            "shapeTexture", &Lighting::VolumetricClouds::shapeTexture,
            "detailTexture", &Lighting::VolumetricClouds::detailTexture,
            "shadowTexture", &Lighting::VolumetricClouds::shadowTexture,
            "coverage", &Lighting::VolumetricClouds::coverage,
            "cloudType", &Lighting::VolumetricClouds::cloudType,
            "cloudSize", &Lighting::VolumetricClouds::cloudSize,
            "verticalDevelopment", &Lighting::VolumetricClouds::verticalDevelopment,
            "detail", &Lighting::VolumetricClouds::detail,
            "densityMultiplier", &Lighting::VolumetricClouds::densityMultiplier,
            "baseAltitude", &Lighting::VolumetricClouds::baseAltitude,
            "layerThickness", &Lighting::VolumetricClouds::layerThickness,
            "variationSeed", &Lighting::VolumetricClouds::variationSeed,
            "quality", &Lighting::VolumetricClouds::quality,
            "distanceLimit", &Lighting::VolumetricClouds::distanceLimit,
            "scattering", &Lighting::VolumetricClouds::scattering,
            "needsNoiseUpdate", &Lighting::VolumetricClouds::needsNoiseUpdate,
            "enable", &Lighting::VolumetricClouds::enable,
            "castShadow", &Lighting::VolumetricClouds::castShadow
            );

        ns->new_usertype<Lighting::Atmosphere>("Atmosphere",
            "new", sol::factories([]() { return CreateRef<Lighting::Atmosphere>(); }),
            "height", &Lighting::Atmosphere::height,
            "rayleighScatteringCoeff", &Lighting::Atmosphere::rayleighScatteringCoeff,
            "groundAlbedo", &Lighting::Atmosphere::groundAlbedo,
            "mieScatteringCoeff", &Lighting::Atmosphere::mieScatteringCoeff,
            "rayleighHeightScale", &Lighting::Atmosphere::rayleighHeightScale,
            "mieHeightScale", &Lighting::Atmosphere::mieHeightScale,
            "sunDiskSize", &Lighting::Atmosphere::sunDiskSize,
            "directLight", &Lighting::Atmosphere::directLight,
            "indirectLight", &Lighting::Atmosphere::indirectLight,
            "probeResolution", sol::property(&Lighting::Atmosphere::GetProbeResolution, &Lighting::Atmosphere::SetProbeResolution),
            "needsUpdate", &Lighting::Atmosphere::needsUpdate
        );

        ns->new_usertype<Lighting::SSS>("SSS",
            "sampleCount", &Lighting::SSS::sampleCount,
            "maxLength", &Lighting::SSS::maxLength,
            "thickness", &Lighting::SSS::thickness,
            "enable", &Lighting::SSS::enable
        );

        ns->new_usertype<Lighting::SSGI>("SSGI",
            "enable", &Lighting::SSGI::enable,
            "enableAo", &Lighting::SSGI::enableAo,
            "radius", &Lighting::SSGI::radius,
            "thickness", &Lighting::SSGI::thickness,
            "rayCount", &Lighting::SSGI::rayCount,
            "sampleCount", &Lighting::SSGI::sampleCount,
            "irradianceLimit", &Lighting::SSGI::irradianceLimit,
            "aoStrength", &Lighting::SSGI::aoStrength,
            "halfResolution", &Lighting::SSGI::halfResolution,
            "ddgi", &Lighting::SSGI::ddgi
        );

        ns->new_usertype<Lighting::RTGI>("RTGI",
            "enable", &Lighting::RTGI::enable,
            "ddgi", &Lighting::RTGI::ddgi,
            "sampleCount", &Lighting::RTGI::sampleCount,
            "halfResolution", &Lighting::RTGI::halfResolution
        );

        ns->new_usertype<Lighting::Reflection>("Reflection",
            "textureLevel", &Lighting::Reflection::textureLevel,
            "radianceLimit", &Lighting::Reflection::radianceLimit,
            "bias", &Lighting::Reflection::bias,
            "spatialFilterStrength", &Lighting::Reflection::spatialFilterStrength,
            "temporalWeight", &Lighting::Reflection::temporalWeight,
            "historyClipMax", &Lighting::Reflection::historyClipMax,
            "currentClipFactor", &Lighting::Reflection::currentClipFactor,
            "enable", &Lighting::Reflection::enable,
            "rt", &Lighting::Reflection::rt,
            "ssr", &Lighting::Reflection::ssr,
            "ddgi", &Lighting::Reflection::ddgi,
            "sampleCount", &Lighting::Reflection::sampleCount,
            "halfResolution", &Lighting::Reflection::halfResolution,
            "useNormalMaps", &Lighting::Reflection::useNormalMaps,
            "opacityCheck", &Lighting::Reflection::opacityCheck
        );

        // A sky probe from a cubemap or an equirectangular .hdr image relative to the asset directory
        ns->new_usertype<Lighting::EnvironmentProbe>("EnvironmentProbe",
            "new", sol::factories([](const std::string& cubemapPath) {
                return CreateRef<Lighting::EnvironmentProbe>(ResourceManager<Texture::Cubemap>::GetOrLoadResourceAsync(cubemapPath));
                }),
            "viewMatrices", &Lighting::EnvironmentProbe::viewMatrices,
            "projectionMatrix", &Lighting::EnvironmentProbe::projectionMatrix,
            "update", &Lighting::EnvironmentProbe::update,
            "cubemap", &Lighting::EnvironmentProbe::cubemap,
            "generatedCubemap", &Lighting::EnvironmentProbe::generatedCubemap,
            "depth", &Lighting::EnvironmentProbe::depth,
            "filteredDiffuse", &Lighting::EnvironmentProbe::filteredDiffuse,
            "filteredSpecular", &Lighting::EnvironmentProbe::filteredSpecular
        );

        ns->new_usertype<Lighting::Fog>("Fog",
            "enable", &Lighting::Fog::enable,
            "albedo", &Lighting::Fog::albedo,
            "density", &Lighting::Fog::density,
            "height", &Lighting::Fog::height,
            "heightFalloff", &Lighting::Fog::heightFalloff,
            "scatteringAnisotropy", &Lighting::Fog::scatteringAnisotropy,
            "ambientFactor", &Lighting::Fog::ambientFactor,
            "froxelTileSize", &Lighting::Fog::froxelTileSize,
            "froxelSliceCount", &Lighting::Fog::froxelSliceCount,
            "froxelFilter", &Lighting::Fog::froxelFilter,
            "froxelFarDistance", &Lighting::Fog::froxelFarDistance,
            "froxelDistributionExponent", &Lighting::Fog::froxelDistributionExponent,
            "volumetricIntensity", &Lighting::Fog::volumetricIntensity
        );

        ns->new_usertype<Lighting::Sky>("Sky",
            "GetProbe", &Lighting::Sky::GetProbe,
            "planetCenter", &Lighting::Sky::planetCenter,
            "planetRadius", &Lighting::Sky::planetRadius,
            // nil removes a feature, e.g. scene.sky.probe = nil
            "clouds", sol::property([](Lighting::Sky& sky) { return sky.clouds; },
                [](Lighting::Sky& sky, sol::optional<Ref<Lighting::VolumetricClouds>> clouds) { sky.clouds = clouds.value_or(nullptr); }),
            "atmosphere", sol::property([](Lighting::Sky& sky) { return sky.atmosphere; },
                [](Lighting::Sky& sky, sol::optional<Ref<Lighting::Atmosphere>> atmosphere) { sky.atmosphere = atmosphere.value_or(nullptr); }),
            "probe", sol::property([](Lighting::Sky& sky) { return sky.probe; },
                [](Lighting::Sky& sky, sol::optional<Ref<Lighting::EnvironmentProbe>> probe) { sky.probe = probe.value_or(nullptr); })
        );

        ns->new_usertype<Lighting::ShadowView>("ShadowView",
            "nearDistance", &Lighting::ShadowView::nearDistance,
            "farDistance", &Lighting::ShadowView::farDistance,
            "viewMatrix", &Lighting::ShadowView::viewMatrix,
            "projectionMatrix", &Lighting::ShadowView::projectionMatrix,
            "frustumMatrix", &Lighting::ShadowView::frustumMatrix,
            "terrainFrustumMatrix", &Lighting::ShadowView::terrainFrustumMatrix
        );

        ns->new_usertype<Lighting::Shadow>("Shadow",
            "SetResolution", &Lighting::Shadow::SetResolution,
            "Update", &Lighting::Shadow::Update,
            "distance", &Lighting::Shadow::distance,
            "longRangeDistance", &Lighting::Shadow::longRangeDistance,
            "bias", &Lighting::Shadow::bias,
            "splitCorrection", &Lighting::Shadow::splitCorrection,
            "edgeSoftness", &Lighting::Shadow::edgeSoftness,
            "cascadeBlendDistance", &Lighting::Shadow::cascadeBlendDistance,
            "resolution", &Lighting::Shadow::resolution,
            "views", &Lighting::Shadow::views,
            "viewCount", &Lighting::Shadow::viewCount,
            "isCascaded", sol::readonly(&Lighting::Shadow::isCascaded),
            "useCubemap", &Lighting::Shadow::useCubemap,
            "allowTerrain", &Lighting::Shadow::allowTerrain,
            "longRange", &Lighting::Shadow::longRange,
            "update", &Lighting::Shadow::update
            );

    }

}
