#include "SSSPanel.h"

namespace Atlas::ImguiExtension {

    void SSSPanel::Render(Ref<Lighting::SSS>& sss) {

        ImGui::PushID(GetNameID());

        ImGui::Checkbox("Enable", &sss->enable);
        ImGui::SliderInt("Sample count", &sss->sampleCount, 2.0, 16.0);
        ImGui::SliderFloat("Max length", &sss->maxLength, 0.01f, 1.0f);
        ImGui::SliderFloat("Thickness", &sss->thickness, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);

        ImGui::PopID();

    }

}