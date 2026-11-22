#include "MaterialGraphParametersPanel.h"

#include "../../Singletons.h"
#include "../../Theme.h"
#include "../Widgets.h"

#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <map>
#include <tuple>

namespace Atlas::Editor::UI {

    void MaterialGraphParametersPanel::Render(Ref<Material>& material) {

        ImGui::PushID(GetNameID());
        textureSelectionPanel.Reset();

        auto compiled = material->graph.IsLoaded() ? material->graph->GetCompiled() : nullptr;
        if (compiled) {
            for (const auto& error : compiled->errors)
                Widgets::Callout(error.message.c_str(), Theme::Error());
            if (!compiled->parameters.empty())
                RenderParameters(*material, *compiled);
            else
                ImGui::TextDisabled("The graph has no parameters");
        }

        // Graphs which only move vertices keep the surface of the material properties
        auto replacesSurface = compiled && compiled->valid && compiled->hasSurface;
        if (!replacesSurface) {
            materialPanel.Render(Singletons::imguiWrapper, material,
                [&](ResourceHandle<Texture::Texture2D> texture) { return textureSelectionPanel.Render(texture); });
        }

        ImGui::PopID();

    }

    void MaterialGraphParametersPanel::RenderParameters(Material& material, const MaterialGraph::CompiledGraph& compiled) {

        using namespace MaterialGraph;

        // Values come before textures, which are larger. Within that the graph decides
        std::vector<const Parameter*> sorted;
        for (const auto& parameter : compiled.parameters)
            sorted.push_back(&parameter);

        std::map<std::string, int32_t> groupOrder;
        for (auto parameter : sorted) {
            auto [iter, inserted] = groupOrder.try_emplace(parameter->group, parameter->order);
            if (!inserted)
                iter->second = std::min(iter->second, parameter->order);
        }

        auto getRank = [](const Parameter& parameter) {
            return parameter.isColor ? 0 : parameter.type == ValueType::Texture ? 2 : 1;
            };

        std::stable_sort(sorted.begin(), sorted.end(), [&](const Parameter* a, const Parameter* b) {
            // Parameters without a group are always on top
            auto aKey = std::tuple(!a->group.empty(), groupOrder[a->group], a->group, a->order, getRank(*a), a->nodeId);
            auto bKey = std::tuple(!b->group.empty(), groupOrder[b->group], b->group, b->order, getRank(*b), b->nodeId);
            return aKey < bKey;
            });

        auto toLower = [](std::string text) {
            std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
            return text;
            };

        if (sorted.size() > maxParametersWithoutSearch) {
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::InputTextWithHint("##Search parameters", "Search parameters...", &parameterSearch);
        }
        else {
            parameterSearch.clear();
        }

        auto search = toLower(parameterSearch);
        auto matches = [&](const Parameter& parameter) {
            return toLower(parameter.name).find(search) != std::string::npos || toLower(parameter.group).find(search) != std::string::npos;
            };

        const auto iconColumnWidth = ImGui::GetTextLineHeight() + 2.0f * ImGui::GetStyle().FramePadding.x;

        for (size_t start = 0; start < sorted.size();) {
            const auto& group = sorted[start]->group;
            auto end = std::find_if(sorted.begin() + start, sorted.end(), [&](const Parameter* parameter) { return parameter->group != group; });
            auto count = std::count_if(sorted.begin() + start, end, [&](const Parameter* parameter) { return matches(*parameter); });

            auto isOpen = count > 0;
            if (count && !group.empty()) {
                // A search shows all results, without changing which groups stay open afterwards
                if (!search.empty()) {
                    ImGui::SeparatorText(group.c_str());
                }
                else {
                    auto label = group + " (" + std::to_string(count) + ")###" + group;
                    auto flags = ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth |
                        (sorted.size() <= maxParametersWithoutSearch ? ImGuiTreeNodeFlags_DefaultOpen : 0);
                    isOpen = ImGui::TreeNodeEx(label.c_str(), flags);
                }
            }

            // The names line up with the labels of the properties below
            if (isOpen && ImGui::BeginTable(("##Parameters" + group).c_str(), 3,
                ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
                ImguiExtension::UIElements::SetupPropertyLabelColumn("Name");
                ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, iconColumnWidth);

                for (auto iter = sorted.begin() + start; iter != end; iter++) {
                    if (matches(**iter))
                        RenderParameter(material, **iter);
                }

                ImGui::EndTable();
            }

            start = size_t(end - sorted.begin());
        }

    }

    void MaterialGraphParametersPanel::RenderParameter(Material& material, const MaterialGraph::Parameter& parameter) {

        using namespace MaterialGraph;

        ImGui::PushID(parameter.name.c_str());
        ImGui::TableNextRow();

        auto isTexture = parameter.type == ValueType::Texture;
        auto isOverridden = isTexture ? material.graphTextures.contains(parameter.name) : material.graphValues.contains(parameter.name);

        // Names of parameters often are one word per capital letter
        std::string name;
        for (size_t i = 0; i < parameter.name.size(); i++) {
            auto character = parameter.name[i];
            if (i > 0 && std::isupper(static_cast<unsigned char>(character)) && std::islower(static_cast<unsigned char>(parameter.name[i - 1])))
                name += ' ';
            name += character;
        }

        // Values of the graph are muted, the ones of this material stand out
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImguiExtension::UIElements::MeasurePropertyLabel(ImGui::CalcTextSize(name.c_str()).x);
        if (isOverridden)
            ImGui::TextUnformatted(name.c_str());
        else
            ImGui::TextColored(Theme::TextMuted(), "%s", name.c_str());
        ImGui::SetItemTooltip("%s\n%s", parameter.name.c_str(), isOverridden ? "This material has its own value" : "Uses the default of the graph");

        ImGui::TableNextColumn();
        if (isTexture) {
            auto changed = false;
            auto selected = textureSelectionPanel.Render(material.GetGraphTexture(parameter), changed);
            if (changed)
                material.SetGraphTexture(parameter.name, selected);
        }
        else {
            auto value = material.GetGraphValue(parameter);
            auto components = int32_t(parameter.type) + 1;

            auto changed = false;
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (parameter.isColor)
                changed = ImGui::ColorEdit3("##Value", &value.x);
            else if (parameter.HasRange())
                changed = ImGui::SliderScalarN("##Value", ImGuiDataType_Float, &value.x, components, &parameter.range.x, &parameter.range.y);
            else
                changed = ImGui::DragScalarN("##Value", ImGuiDataType_Float, &value.x, components, 0.01f);

            if (changed)
                material.SetGraphParameter(parameter.name, value);
        }

        ImGui::TableNextColumn();
        if (isOverridden && Widgets::IconButton("Reset", Singletons::icons->Get(IconType::Reset), ImGui::GetTextLineHeight(), "Back to the default of the graph")) {
            material.ResetGraphParameter(parameter.name);
        }

        ImGui::PopID();

    }

}
