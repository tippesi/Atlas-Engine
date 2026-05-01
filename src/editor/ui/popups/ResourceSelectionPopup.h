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
        ResourceHandle<T> Render(std::vector<ResourceHandle<T>> resources, ImVec2 size = ImVec2(-FLT_MAX, 200.0f)) {

            ResourceHandle<T> handle;

            ImGui::SetNextWindowSize(size);

            if (ImGui::BeginPopup(GetNameID())) {

                if (wasJustOpened) {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::InputTextWithHint("Search", "Type to search for loaded resource", &resourceSearch);
                
                ImGui::BeginChild("Resource list");

                resources = ApplySearchAndSortFiltering(resources);

                for (size_t i = 0; i < resources.size(); i++) {
                    bool isSelected = false;
                    const auto& resource = resources[i].GetResource();

                    auto itemName = resource->GetFileName();

                    ImGui::Selectable(itemName.c_str(), &isSelected, ImGuiSelectableFlags_SpanAvailWidth);

                    if (isSelected)
                        handle = resources[i];
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