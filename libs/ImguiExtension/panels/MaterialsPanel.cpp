#include "MaterialsPanel.h"

#include <imgui_stdlib.h>

#include <ranges>

namespace Atlas::ImguiExtension {

    void MaterialsPanel::Render(Ref<ImguiWrapper>& wrapper, std::vector<Ref<Material>> &materials) {

        ImGui::PushID(GetNameID());

        ImGui::InputTextWithHint("Search", "Type to search material", &materialSearch);

        auto matches = materials | std::views::filter(
            [&](const Ref<Material>& material) {
                return material->name.find(materialSearch) != std::string::npos;
            });

        int32_t counter = 0;
        for (auto& material : matches) {

            ImGui::PushID(counter++);

            auto materialName = !material->name.empty() ? material->name
                : "No name " + std::to_string(counter);

            if (ImGui::TreeNode(materialName.c_str())) {

                materialPanel.Render(wrapper, material);

                ImGui::TreePop();

            }

            ImGui::PopID();

        }

        ImGui::PopID();

    }

}