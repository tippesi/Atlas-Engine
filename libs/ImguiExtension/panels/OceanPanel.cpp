#include "OceanPanel.h"

#include "ocean/OceanSerializer.h"

namespace Atlas::ImguiExtension {

    void OceanPanel::Render(Ref<Ocean::Ocean>& ocean) {

        ImGui::PushID(GetNameID());

        auto& simulation = ocean->simulation;

        ImGui::Checkbox("Enable", &ocean->enable);
        ImGui::Checkbox("Underwater shader", &ocean->underwaterShader);
        ImGui::Checkbox("Wireframe", &ocean->wireframe);
        ImGui::DragFloat3("Translation", glm::value_ptr(ocean->translation), 0.1f);

        // Changing these recreates the ocean, the other settings are kept
        ImGui::SeparatorText("Resolution");
        int32_t LoDCount = ocean->GetLoDCount();
        float size = ocean->sideLength;
        int32_t N = simulation.N;

        bool recreate = false;
        const char* resolutions[] = { "128", "256", "512", "1024" };
        int32_t resolutionIdx = glm::clamp(int32_t(log2f(float(N))) - 7, 0, 3);
        if (ImGui::Combo("Simulation resolution", &resolutionIdx, resolutions, IM_ARRAYSIZE(resolutions))) {
            N = 128 << resolutionIdx;
            recreate = true;
        }
        ImGui::DragFloat("Size", &size, 16.0f, 64.0f, 65536.0f, "%.0f m");
        recreate |= ImGui::IsItemDeactivatedAfterEdit();
        ImGui::DragInt("LoD count", &LoDCount, 0.1f, 1, 12);
        recreate |= ImGui::IsItemDeactivatedAfterEdit();

        if (recreate) {
            json j = *ocean;
            j["LoDCount"] = LoDCount;
            j["size"] = size;
            j["N"] = N;
            ocean = CreateRef<Ocean::Ocean>(j.get<Ocean::Ocean>());
        }

        ImGui::SeparatorText("Waves");
        bool spectrumChanged = false;
        spectrumChanged |= ImGui::DragFloat("Wave amplitude", &simulation.waveAmplitude, 0.00001f,
            0.0f, 0.01f, "%.6f");
        spectrumChanged |= ImGui::DragFloat("Small wave surpression", &simulation.waveSurpression, 0.0001f,
            0.0f, 1.0f, "%.4f");
        spectrumChanged |= ImGui::DragFloat2("Wind direction", glm::value_ptr(simulation.windDirection),
            0.01f, -1.0f, 1.0f);
        spectrumChanged |= ImGui::DragFloat("Wind speed", &simulation.windSpeed, 0.1f, 0.5f, 40.0f, "%.1f m/s");
        spectrumChanged |= ImGui::SliderFloat("Wind dependency", &simulation.windDependency, 0.0f, 1.0f);
        ImGui::SetItemTooltip("Strength of the waves running against the wind");
        if (spectrumChanged)
            simulation.UpdateSpectrum();

        ImGui::DragFloat("Displacement scale", &ocean->displacementScale, 0.01f, 0.0f, 4.0f);
        ImGui::DragFloat("Choppyness scale", &ocean->choppynessScale, 0.01f, 0.0f, 2.0f);
        ImGui::DragFloat("Patch size", &ocean->tiling, 0.1f, 1.0f, 1024.0f, "%.1f m");
        ImGui::SetItemTooltip("Size of the smallest cascade, the others are scaled by their tiling factors");
        ImGui::DragFloat("Simulation speed", &simulation.simulationSpeed, 0.01f, 0.0f, 10.0f);
        ImGui::Checkbox("Update simulation", &simulation.update);
        ImGui::Checkbox("Normals from FFT", &simulation.fftNormals);

        ImGui::SeparatorText("Spectrum cascades");
        ImGui::DragFloat4("Tiling factors", glm::value_ptr(simulation.spectrumTilingFactors), 0.1f, 0.1f, 100.0f);
        ImGui::DragFloat4("Weights", glm::value_ptr(simulation.spectrumWeights), 0.01f, 0.0f, 1.0f);
        ImGui::DragFloat4("Fadeout distances", glm::value_ptr(simulation.spectrumFadeoutDistances),
            10.0f, 0.0f, 100000.0f, "%.0f");

        ImGui::SeparatorText("Foam");
        ImGui::DragFloat("Foam offset", &simulation.foamOffset, 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat("Foam scale", &simulation.foamScale, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Foam size", &simulation.foamSize, 0.01f, 0.0f, 20.0f);

        ImGui::SeparatorText("Water color");
        ImGui::ColorEdit3("Water body color", glm::value_ptr(ocean->waterBodyColor));
        ImGui::ColorEdit3("Deep water body color", glm::value_ptr(ocean->deepWaterBodyColor));
        ImGui::ColorEdit3("Scatter color", glm::value_ptr(ocean->scatterColor));
        ImGui::DragFloat2("Water color intensity", glm::value_ptr(ocean->waterColorIntensity), 0.01f, 0.0f, 10.0f);

        ImGui::SeparatorText("Shore waves");
        ImGui::DragFloat("Distance offset", &ocean->shoreWaveDistanceOffset, 0.5f, 0.0f, 1000.0f);
        ImGui::DragFloat("Distance scale", &ocean->shoreWaveDistanceScale, 0.5f, 0.0f, 1000.0f);
        ImGui::DragFloat("Amplitude", &ocean->shoreWaveAmplitude, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Steepness", &ocean->shoreWaveSteepness, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Power", &ocean->shoreWavePower, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat("Speed", &ocean->shoreWaveSpeed, 0.01f, 0.0f, 50.0f);
        ImGui::DragFloat("Length", &ocean->shoreWaveLength, 0.01f, 0.0f, 50.0f);

        ImGui::PopID();

    }

}
