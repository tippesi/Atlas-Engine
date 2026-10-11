#pragma once

#include "Popup.h"

#include "resource/Resource.h"

#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>

namespace Atlas::Editor::UI {

    class ResourceSelectionPopup : public Popup {

    public:
        // Idea: This whole class could be changed to a content selection popup, such that it would work with
        // the content browser content as well
        typedef enum ResourceOriginBits {
            ResourceManagerBit = (1 << 0),
            ContentBrowserBit = (1 << 1)
        }ResourceOriginBits;

        typedef int32_t ResourceOrigin;

        ResourceSelectionPopup() : Popup("ResourceSelectionPopup") {}

        template<class T>
        ResourceHandle<T> Render(std::vector<ResourceHandle<T>> resources, ImVec2 size = ImVec2(-FLT_MAX, 260.0f)) {

            ResourceHandle<T> handle;

            // A negative width means automatic, which is a width that fits typical file names
            if (size.x < 0.0f)
                size.x = ImGui::GetFontSize() * 24.0f;
            ImGui::SetNextWindowSize(size);

            if (ImGui::BeginPopup(GetNameID())) {

                if (wasJustOpened) {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::SetNextItemWidth(-FLT_MIN);
                bool confirmFirst = ImGui::InputTextWithHint("##Search", "Search loaded resources...",
                    &resourceSearch, ImGuiInputTextFlags_EnterReturnsTrue);

                resources = ApplySearchAndSortFiltering(resources);

                ImGui::TextDisabled("%d %s", int32_t(resources.size()), resources.size() == 1 ? "resource" : "resources");

                ImGui::BeginChild("Resource list", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

                if (resources.empty())
                    ImGui::TextDisabled("%s", resourceSearch.empty() ? "No resources of this type are loaded" :
                        "No resources match the search");

                // Pressing enter in the search field picks the first match
                if (confirmFirst && !resources.empty())
                    handle = resources.front();

                ImGuiListClipper clipper;
                clipper.Begin(int32_t(resources.size()));
                while (clipper.Step()) {
                    for (int32_t i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                        bool isSelected = false;
                        const auto& resource = resources[i].GetResource();

                        auto itemName = resource->GetFileName();

                        ImGui::PushID(i);
                        ImGui::Selectable(itemName.c_str(), &isSelected, ImGuiSelectableFlags_SpanAvailWidth);
                        ImGui::SetItemTooltip("%s", resource->path.c_str());
                        ImGui::PopID();

                        if (isSelected)
                            handle = resources[i];
                    }
                }

                ImGui::EndChild();

                wasJustOpened = false;

                if (handle.IsValid())
                    ImGui::CloseCurrentPopup();

                ImGui::EndPopup();

            }

            return handle;

        }

        static inline ResourceOrigin resourceOrigin = ResourceOriginBits::ResourceManagerBit;

    private:
        template<class T>
        std::vector<ResourceHandle<T>> ApplySearchAndSortFiltering(const std::vector<ResourceHandle<T>>& resources) {

            std::vector<ResourceHandle<T>> filteredResources;

            if (!resourceSearch.empty()) {
                std::string searchQuery = resourceSearch;
                std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);

                for (const auto& handle : resources) {
                    auto filename = handle.GetResource()->GetFileName();
                    std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

                    // Filter out by search query, if it is a valid (non-empty) query
                    if (searchQuery.empty() || filename.find(searchQuery) != std::string::npos)
                        filteredResources.push_back(handle);
                }
            }
            else {
                filteredResources = resources;
            }

            std::sort(filteredResources.begin(), filteredResources.end(),
                [](const ResourceHandle<T>& res0, const ResourceHandle<T>& res1) {
                    auto res0Filename = res0.GetResource()->GetFileName();
                    auto res1Filename = res1.GetResource()->GetFileName();

                    return res0Filename < res1Filename;
                });

            return filteredResources;

        }

        std::string resourceSearch;

    };

}