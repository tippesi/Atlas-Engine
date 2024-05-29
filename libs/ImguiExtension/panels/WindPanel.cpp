#include "WindPanel.h"

namespace Atlas::ImguiExtension {

	void WindPanel::Render(Ref<ImguiWrapper>& wrapper, Scene::Wind& wind) {

        ImGui::PushID(GetNameID());

        ImGui::Text("Noise texture");

        UIElements::TextureView(wrapper, wind.noiseMap, 255.0f);

        ImGui::Separator();

        ImGui::DragFloat2("Wind direction", glm::value_ptr(wind.direction), 0.01f, -1.0f, 1.0f);
        ImGui::DragFloat("Wind speed", &wind.speed, 0.5f, 0.0f, 100.0f);

        ImGui::PopID();

	}

}