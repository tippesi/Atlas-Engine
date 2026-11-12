#include "OceanPanel.h"

#include "ocean/OceanSerializer.h"

namespace Atlas::ImguiExtension {

    void OceanPanel::Render(Ref<Ocean::Ocean>& ocean, TextureSelector textureSelector) {

        ImGui::PushID(GetNameID());

        auto& simulation = ocean->simulation;

        if (UIElements::BeginProperties("OceanGeneral")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &ocean->enable);
            UIElements::PropertyLabel("Underwater shader");
            ImGui::Checkbox("##Underwater shader", &ocean->underwaterShader);
            UIElements::PropertyLabel("Wireframe");
            ImGui::Checkbox("##Wireframe", &ocean->wireframe);
            UIElements::PropertyLabel("Translation");
            ImGui::DragFloat3("##Translation", glm::value_ptr(ocean->translation), 0.1f);
            UIElements::EndProperties();
        }

        // Changing these recreates the ocean, the other settings are kept
        ImGui::SeparatorText("Resolution");
        int32_t LoDCount = ocean->GetLoDCount();
        float size = ocean->sideLength;
        int32_t N = simulation.N;

        bool recreate = false;
        if (UIElements::BeginProperties("OceanResolution")) {
            const char* resolutions[] = { "128", "256", "512", "1024" };
            int32_t resolutionIdx = glm::clamp(int32_t(log2f(float(N))) - 7, 0, 3);
            UIElements::PropertyLabel("Simulation resolution");
            if (ImGui::Combo("##Simulation resolution", &resolutionIdx, resolutions, IM_ARRAYSIZE(resolutions))) {
                N = 128 << resolutionIdx;
                recreate = true;
            }
            UIElements::PropertyLabel("Size");
            recreate |= ImGui::DragFloat("##Size", &size, 16.0f, 64.0f, 65536.0f, "%.0f m");
            UIElements::PropertyLabel("LoD count");
            recreate |= ImGui::DragInt("##LoD count", &LoDCount, 0.1f, 1, 12);
            UIElements::EndProperties();
        }

        if (recreate) {
            json j = *ocean;
            j["LoDCount"] = LoDCount;
            j["size"] = size;
            j["N"] = N;
            ocean = CreateRef<Ocean::Ocean>(j.get<Ocean::Ocean>());
        }

        ImGui::SeparatorText("Waves");
        if (UIElements::BeginProperties("OceanWaves")) {
            bool spectrumChanged = false;
            UIElements::PropertyLabel("Wave amplitude");
            spectrumChanged |= ImGui::DragFloat("##Wave amplitude", &simulation.waveAmplitude, 0.00001f,
                0.0f, 0.01f, "%.6f");
            UIElements::PropertyLabel("Small wave surpression");
            spectrumChanged |= ImGui::DragFloat("##Small wave surpression", &simulation.waveSurpression, 0.0001f,
                0.0f, 1.0f, "%.4f");
            UIElements::PropertyLabel("Wind direction");
            spectrumChanged |= ImGui::DragFloat2("##Wind direction", glm::value_ptr(simulation.windDirection),
                0.01f, -1.0f, 1.0f);
            UIElements::PropertyLabel("Wind speed");
            spectrumChanged |= ImGui::DragFloat("##Wind speed", &simulation.windSpeed, 0.1f, 0.5f, 40.0f, "%.1f m/s");
            UIElements::PropertyLabel("Wind dependency", "Strength of the waves running against the wind");
            spectrumChanged |= ImGui::SliderFloat("##Wind dependency", &simulation.windDependency, 0.0f, 1.0f);
            if (spectrumChanged)
                simulation.UpdateSpectrum();

            UIElements::PropertyLabel("Displacement scale");
            ImGui::DragFloat("##Displacement scale", &ocean->displacementScale, 0.01f, 0.0f, 4.0f);
            UIElements::PropertyLabel("Choppyness scale");
            ImGui::DragFloat("##Choppyness scale", &ocean->choppynessScale, 0.01f, 0.0f, 2.0f);
            UIElements::PropertyLabel("Patch size", "Size of the smallest cascade, the others are scaled by their tiling factors");
            ImGui::DragFloat("##Patch size", &ocean->tiling, 0.1f, 1.0f, 1024.0f, "%.1f m");
            UIElements::PropertyLabel("Simulation speed");
            ImGui::DragFloat("##Simulation speed", &simulation.simulationSpeed, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Update simulation");
            ImGui::Checkbox("##Update simulation", &simulation.update);
            UIElements::PropertyLabel("Normals from FFT");
            ImGui::Checkbox("##Normals from FFT", &simulation.fftNormals);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Spectrum cascades");
        if (UIElements::BeginProperties("OceanSpectrumCascades")) {
            UIElements::PropertyLabel("Tiling factors");
            ImGui::DragFloat4("##Tiling factors", glm::value_ptr(simulation.spectrumTilingFactors), 0.1f, 0.1f, 100.0f);
            UIElements::PropertyLabel("Weights");
            ImGui::DragFloat4("##Weights", glm::value_ptr(simulation.spectrumWeights), 0.01f, 0.0f, 1.0f);
            UIElements::PropertyLabel("Fadeout distances");
            ImGui::DragFloat4("##Fadeout distances", glm::value_ptr(simulation.spectrumFadeoutDistances),
                10.0f, 0.0f, 100000.0f, "%.0f");
            UIElements::EndProperties();
        }

        if (textureSelector.has_value()) {
            ImGui::SeparatorText("Ripple normal map");
            ImGui::PushID("Ripple");
            ocean->rippleTexture = textureSelector.value()(ocean->rippleTexture);
            ImGui::PopID();
        }

        ImGui::SeparatorText("Foam");
        if (textureSelector.has_value()) {
            ImGui::PushID("Foam");
            ocean->foamTexture = textureSelector.value()(ocean->foamTexture);
            ImGui::PopID();
        }
        if (UIElements::BeginProperties("OceanFoam")) {
            UIElements::PropertyLabel("Foam offset");
            ImGui::DragFloat("##Foam offset", &simulation.foamOffset, 0.01f, -1.0f, 1.0f);
            UIElements::PropertyLabel("Foam scale");
            ImGui::DragFloat("##Foam scale", &simulation.foamScale, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Foam size");
            ImGui::DragFloat("##Foam size", &simulation.foamSize, 0.01f, 0.0f, 20.0f);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Water color");
        if (UIElements::BeginProperties("OceanWaterColor")) {
            UIElements::PropertyLabel("Water body color");
            ImGui::ColorEdit3("##Water body color", glm::value_ptr(ocean->waterBodyColor));
            UIElements::PropertyLabel("Scatter color");
            ImGui::ColorEdit3("##Scatter color", glm::value_ptr(ocean->scatterColor));
            UIElements::PropertyLabel("Water color intensity");
            ImGui::DragFloat2("##Water color intensity", glm::value_ptr(ocean->waterColorIntensity), 0.01f, 0.0f, 10.0f);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Shore waves");
        if (UIElements::BeginProperties("OceanShoreWaves")) {
            UIElements::PropertyLabel("Distance offset");
            ImGui::DragFloat("##Distance offset", &ocean->shoreWaveDistanceOffset, 0.5f, 0.0f, 1000.0f);
            UIElements::PropertyLabel("Distance scale");
            ImGui::DragFloat("##Distance scale", &ocean->shoreWaveDistanceScale, 0.5f, 0.0f, 1000.0f);
            UIElements::PropertyLabel("Amplitude");
            ImGui::DragFloat("##Amplitude", &ocean->shoreWaveAmplitude, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Steepness");
            ImGui::DragFloat("##Steepness", &ocean->shoreWaveSteepness, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Power");
            ImGui::DragFloat("##Power", &ocean->shoreWavePower, 0.01f, 0.0f, 10.0f);
            UIElements::PropertyLabel("Speed");
            ImGui::DragFloat("##Speed", &ocean->shoreWaveSpeed, 0.01f, 0.0f, 50.0f);
            UIElements::PropertyLabel("Length");
            ImGui::DragFloat("##Length", &ocean->shoreWaveLength, 0.01f, 0.0f, 50.0f);
            UIElements::EndProperties();
        }

        ImGui::SeparatorText("Physics");
        if (UIElements::BeginProperties("OceanPhysics")) {
            UIElements::PropertyLabel("Enable physics", "Bodies with buoyancy float on the waves and scripts can query the surface");
            ImGui::Checkbox("##Enable physics", &ocean->physics);
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

}
