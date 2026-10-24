#include "OceanSerializer.h"

namespace Atlas::Ocean {

    void to_json(json& j, const Ocean& p) {
        j = json {
            {"LoDCount", p.GetLoDCount()},
            {"size", p.sideLength},
            {"N", p.simulation.N},
            {"translation", p.translation},
            {"enable", p.enable},
            {"underwaterShader", p.underwaterShader},
            {"waterBodyColor", p.waterBodyColor},
            {"deepWaterBodyColor", p.deepWaterBodyColor},
            {"scatterColor", p.scatterColor},
            {"waterColorIntensity", p.waterColorIntensity},
            {"displacementScale", p.displacementScale},
            {"choppynessScale", p.choppynessScale},
            {"tiling", p.tiling},
            {"shoreWaveDistanceOffset", p.shoreWaveDistanceOffset},
            {"shoreWaveDistanceScale", p.shoreWaveDistanceScale},
            {"shoreWaveAmplitude", p.shoreWaveAmplitude},
            {"shoreWaveSteepness", p.shoreWaveSteepness},
            {"shoreWavePower", p.shoreWavePower},
            {"shoreWaveSpeed", p.shoreWaveSpeed},
            {"shoreWaveLength", p.shoreWaveLength},
            {"spectrumTilingFactors", p.simulation.spectrumTilingFactors},
            {"spectrumWeights", p.simulation.spectrumWeights},
            {"spectrumFadeoutDistances", p.simulation.spectrumFadeoutDistances},
            {"waveAmplitude", p.simulation.waveAmplitude},
            {"waveSurpression", p.simulation.waveSurpression},
            {"windDirection", p.simulation.windDirection},
            {"windSpeed", p.simulation.windSpeed},
            {"windDependency", p.simulation.windDependency},
            {"foamOffset", p.simulation.foamOffset},
            {"foamScale", p.simulation.foamScale},
            {"foamSize", p.simulation.foamSize},
            {"simulationSpeed", p.simulation.simulationSpeed},
            {"update", p.simulation.update},
            {"fftNormals", p.simulation.fftNormals}
        };
    }

    void from_json(const json& j, Ocean& p) {
        auto LoDCount = j.value("LoDCount", 9);
        auto size = j.value("size", 4096.0f);
        auto N = j.value("N", 512);

        // Only these need a new ocean, everything else is changed in place
        if (!p.simulation.displacementMap.IsValid() || p.GetLoDCount() != LoDCount ||
            p.sideLength != size || p.simulation.N != N)
            p = Ocean(LoDCount, size, vec3(0.0f), N);

        try_get_json(j, "translation", p.translation);
        try_get_json(j, "enable", p.enable);
        try_get_json(j, "underwaterShader", p.underwaterShader);
        try_get_json(j, "waterBodyColor", p.waterBodyColor);
        try_get_json(j, "deepWaterBodyColor", p.deepWaterBodyColor);
        try_get_json(j, "scatterColor", p.scatterColor);
        try_get_json(j, "waterColorIntensity", p.waterColorIntensity);
        try_get_json(j, "displacementScale", p.displacementScale);
        try_get_json(j, "choppynessScale", p.choppynessScale);
        try_get_json(j, "tiling", p.tiling);
        try_get_json(j, "shoreWaveDistanceOffset", p.shoreWaveDistanceOffset);
        try_get_json(j, "shoreWaveDistanceScale", p.shoreWaveDistanceScale);
        try_get_json(j, "shoreWaveAmplitude", p.shoreWaveAmplitude);
        try_get_json(j, "shoreWaveSteepness", p.shoreWaveSteepness);
        try_get_json(j, "shoreWavePower", p.shoreWavePower);
        try_get_json(j, "shoreWaveSpeed", p.shoreWaveSpeed);
        try_get_json(j, "shoreWaveLength", p.shoreWaveLength);
        try_get_json(j, "spectrumTilingFactors", p.simulation.spectrumTilingFactors);
        try_get_json(j, "spectrumWeights", p.simulation.spectrumWeights);
        try_get_json(j, "spectrumFadeoutDistances", p.simulation.spectrumFadeoutDistances);
        try_get_json(j, "waveAmplitude", p.simulation.waveAmplitude);
        try_get_json(j, "waveSurpression", p.simulation.waveSurpression);
        try_get_json(j, "windDirection", p.simulation.windDirection);
        try_get_json(j, "windSpeed", p.simulation.windSpeed);
        try_get_json(j, "windDependency", p.simulation.windDependency);
        try_get_json(j, "foamOffset", p.simulation.foamOffset);
        try_get_json(j, "foamScale", p.simulation.foamScale);
        try_get_json(j, "foamSize", p.simulation.foamSize);
        try_get_json(j, "simulationSpeed", p.simulation.simulationSpeed);
        try_get_json(j, "update", p.simulation.update);
        try_get_json(j, "fftNormals", p.simulation.fftNormals);

        p.simulation.UpdateSpectrum();
    }

}
