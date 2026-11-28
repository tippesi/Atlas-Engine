#include "SSSPanel.h"

namespace Atlas::ImguiExtension {

    void SSSPanel::Render(Ref<Lighting::SSS>& sss) {

        ImGui::PushID(GetNameID());

        if (UIElements::BeginProperties("SSSGeneral")) {
            UIElements::PropertyLabel("Enable");
            ImGui::Checkbox("##Enable", &sss->enable);
            UIElements::PropertyLabel("Trace in world space");
            ImGui::Checkbox("##Trace in world space", &sss->traceWorldSpace);
            UIElements::PropertyLabel("Sample count");
            ImGui::DragInt("##Sample count", &sss->sampleCount, 1.f, 2, 64);
            UIElements::PropertyLabel("Max length");
            ImGui::SliderFloat("##Max length", &sss->maxLength, 0.01f, 1.0f);
            UIElements::PropertyLabel("Min length world space");
            ImGui::SliderFloat("##Min length world space", &sss->minLengthWorldSpace, 0.01f, 1.0f);
            UIElements::PropertyLabel("Thickness");
            ImGui::SliderFloat("##Thickness", &sss->thickness, 0.001f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
            UIElements::EndProperties();
        }

        ImGui::PopID();

    }

}
