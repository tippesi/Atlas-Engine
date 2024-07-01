#include "LightingBindings.h"

#include "lighting/Shadow.h"
#include "lighting/AO.h"
#include "lighting/SSGI.h"
#include "lighting/EnvironmentProbe.h"
#include "lighting/SSS.h"
#include "lighting/IrradianceVolume.h"
#include "lighting/Atmosphere.h"
#include "lighting/Reflection.h"
#include "lighting/Fog.h"
#include "lighting/VolumetricClouds.h"
#include "lighting/Sky.h"

namespace Atlas::Scripting::Bindings {

    void GenerateLightingBindings(sol::table* ns) {

        ns->new_usertype<Lighting::VolumetricClouds::Scattering>("VolumetricCloudsScattering",
            "extinctionFactor", &Lighting::VolumetricClouds::Scattering::extinctionFactor,
            "scatteringFactor", &Lighting::VolumetricClouds::Scattering::scatteringFactor,
            "extinctionCoefficients", &Lighting::VolumetricClouds::Scattering::extinctionCoefficients,
            "eccentricityFirstPhase", &Lighting::VolumetricClouds::Scattering::eccentricityFirstPhase,
            "eccentricitySecondPhase", &Lighting::VolumetricClouds::Scattering::eccentricitySecondPhase,
            "phaseAlpha", &Lighting::VolumetricClouds::Scattering::phaseAlpha
            );

        ns->new_usertype<Lighting::VolumetricClouds>("VolumetricClouds",
            "coverageTexture", &Lighting::VolumetricClouds::coverageTexture,
            "shapeTexture", &Lighting::VolumetricClouds::shapeTexture,
            "detailTexture", &Lighting::VolumetricClouds::detailTexture,
            "shadowTexture", &Lighting::VolumetricClouds::shadowTexture,
            "sampleCount", &Lighting::VolumetricClouds::sampleCount,
            "occlusionSampleCount", &Lighting::VolumetricClouds::occlusionSampleCount,
            "shadowSampleFraction", &Lighting::VolumetricClouds::shadowSampleFraction,
            "minHeight", &Lighting::VolumetricClouds::minHeight,
            "maxHeight", &Lighting::VolumetricClouds::maxHeight,
            "distanceLimit", &Lighting::VolumetricClouds::distanceLimit,
            "coverageScale", &Lighting::VolumetricClouds::coverageScale,
            "shapeScale", &Lighting::VolumetricClouds::shapeScale,
            "detailScale", &Lighting::VolumetricClouds::detailScale,
            "coverageSpeed", &Lighting::VolumetricClouds::coverageSpeed,
            "shapeSpeed", &Lighting::VolumetricClouds::shapeSpeed,
            "detailSpeed", &Lighting::VolumetricClouds::detailSpeed,
            "detailStrength", &Lighting::VolumetricClouds::detailStrength,
            "densityMultiplier", &Lighting::VolumetricClouds::densityMultiplier,
            "heightStretch", &Lighting::VolumetricClouds::heightStretch,
            "darkEdgeFocus", &Lighting::VolumetricClouds::darkEdgeFocus,
            "darkEdgeAmbient", &Lighting::VolumetricClouds::darkEdgeAmbient,
            "needsNoiseUpdate", &Lighting::VolumetricClouds::needsNoiseUpdate,
            "enable", &Lighting::VolumetricClouds::enable,
            "castShadow", &Lighting::VolumetricClouds::castShadow,
            "stochasticOcclusionSampling", &Lighting::VolumetricClouds::stochasticOcclusionSampling,
            "scattering", &Lighting::VolumetricClouds::scattering
            );

        ns->new_usertype<Lighting::Atmosphere>("Atmosphere",
            "height", &Lighting::Atmosphere::height
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
            "rayCount", &Lighting::SSGI::rayCount,
            "sampleCount", &Lighting::SSGI::sampleCount,
            "irradianceLimit", &Lighting::SSGI::irradianceLimit,
            "aoStrength", &Lighting::SSGI::aoStrength,
            "rt", &Lighting::SSGI::rt,
            "opacityCheck", &Lighting::SSGI::opacityCheck
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
            "gi", &Lighting::Reflection::gi,
            "useShadowMap", &Lighting::Reflection::useShadowMap,
            "useNormalMaps", &Lighting::Reflection::useNormalMaps,
            "opacityCheck", &Lighting::Reflection::opacityCheck
        );

        ns->new_usertype<Lighting::EnvironmentProbe>("EnvironmentProbe",
            "resolution", &Lighting::EnvironmentProbe::resolution,
            "viewMatrices", &Lighting::EnvironmentProbe::viewMatrices,
            "projectionMatrix", &Lighting::EnvironmentProbe::projectionMatrix,
            "update", &Lighting::EnvironmentProbe::update,
            "cubemap", &Lighting::EnvironmentProbe::cubemap,
            "depth", &Lighting::EnvironmentProbe::depth,
            "filteredDiffuse", &Lighting::EnvironmentProbe::filteredDiffuse,
            "filteredSpecular", &Lighting::EnvironmentProbe::filteredSpecular
        );

        ns->new_usertype<Lighting::Fog>("Fog",
            "enable", &Lighting::Fog::enable,
            "extinctionFactor", &Lighting::Fog::extinctionFactor,
            "scatteringFactor", &Lighting::Fog::scatteringFactor,
            "extinctionCoefficients", &Lighting::Fog::extinctionCoefficients,
            "density", &Lighting::Fog::density,
            "height", &Lighting::Fog::height,
            "heightFalloff", &Lighting::Fog::heightFalloff,
            "scatteringAnisotropy", &Lighting::Fog::scatteringAnisotropy,
            "ambientFactor", &Lighting::Fog::ambientFactor,
            "rayMarching", &Lighting::Fog::rayMarching,
            "rayMarchStepCount", &Lighting::Fog::rayMarchStepCount,
            "volumetricIntensity", &Lighting::Fog::volumetricIntensity
        );

        ns->new_usertype<Lighting::Sky>("Sky",
            "GetProbe", &Lighting::Sky::GetProbe,
            "planetCenter", &Lighting::Sky::planetCenter,
            "planetRadius", &Lighting::Sky::planetRadius,
            "clouds", &Lighting::Sky::clouds,
            "atmosphere", &Lighting::Sky::atmosphere,
            "probe", &Lighting::Sky::probe
        );

    }

}