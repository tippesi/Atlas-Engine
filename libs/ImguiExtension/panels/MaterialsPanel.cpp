#include "MaterialsPanel.h"

#include <imgui_stdlib.h>

#include <ranges>
#include <algorithm>

namespace Atlas::ImguiExtension {

    void MaterialsPanel::Render(Ref<ImguiWrapper>& wrapper, std::vector<ResourceHandle<Material>>& materials,
        MaterialSelector materialSelector, MaterialPanel::TextureSelector textureSelector,
        GraphMaterialRenderer graphMaterialRenderer) {

        ImGui::PushID(GetNameID());

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##Search", "Search materials...", &materialSearch);

        auto matches = materials | std::views::filter(
            [&](const ResourceHandle<Material>& material) {
                if (!material.IsLoaded() && materialSearch.empty())
                    return true;
                else if (!material.IsLoaded() && !materialSearch.empty())
                    return false;

                return material->name.find(materialSearch) != std::string::npos;
            });

        int32_t counter = 0;
        for (auto& material : matches) {

            ImGui::PushID(counter++);

            std::string materialName;
            if (material.IsLoaded()) {
                materialName = !material->name.empty() ? material->name
                    : "No name for material " + std::to_string(counter);
            }
            else {
                materialName = "No material";
            }

            auto nodeID = GetNameID() + std::to_string(counter);
            materialName += "###" + nodeID;

            if (UIElements::BeginPropertyGroup(materialName.c_str())) {
                if (ImGui::TreeNodeEx(materialName.c_str(), UIElements::PropertyGroupHeaderFlags())) {
                    UIElements::BeginPropertyContent();
                    if (materialSelector.has_value()) {
                        material = materialSelector.value()(material);
                        ImGui::Separator();
                    }
                    // Might be deleted above
                    if (material.IsLoaded() && material->HasGraph() && graphMaterialRenderer.has_value()) {
                        graphMaterialRenderer.value()(material.Get());
                    }
                    else if (material.IsLoaded()) {
                        materialPanel.Render(wrapper, material.Get(), textureSelector);
                    }
                    UIElements::EndPropertyContent();

                    ImGui::TreePop();
                }
                UIElements::EndPropertyGroup();
            }

            ImGui::PopID();

        }

        ImGui::PopID();

    }

    void MaterialsPanel::Render(Ref<ImguiWrapper>& wrapper, std::vector<Ref<Material>>& materials,
        MaterialPanel::TextureSelector textureSelector) {

        ImGui::PushID(GetNameID());

        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##Search", "Search materials...", &materialSearch);

        auto matches = materials | std::views::filter(
            [&](const Ref<Material>& material) {
                return material->name.find(materialSearch) != std::string::npos;
            });

        int32_t counter = 0;
        for (auto& material : matches) {

            ImGui::PushID(counter++);

            auto materialName = !material->name.empty() ? material->name
                : "No name " + std::to_string(counter);
            auto nodeID = GetNameID() + std::to_string(counter);
            materialName += "###" + nodeID;

            if (UIElements::BeginPropertyGroup(materialName.c_str())) {
                if (ImGui::TreeNodeEx(materialName.c_str(), UIElements::PropertyGroupHeaderFlags())) {
                    UIElements::BeginPropertyContent();
                    materialPanel.Render(wrapper, material, textureSelector);
                    UIElements::EndPropertyContent();

                    ImGui::TreePop();
                }
                UIElements::EndPropertyGroup();
            }

            ImGui::PopID();

        }

        ImGui::PopID();

    }

}