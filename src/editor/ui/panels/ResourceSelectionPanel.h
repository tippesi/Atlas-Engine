#pragma once

#include "Panel.h"
#include "Singletons.h"
#include "ui/popups/ResourceSelectionPopup.h"
#include "tools/ResourcePayloadHelper.h"

namespace Atlas::Editor::UI {

	template<class T>
	class ResourceSelectionPanel : Panel {

    public:
		ResourceSelectionPanel() : Panel("ResourceSelectionPanel") {}

        ResourceHandle<T> Render(ResourceHandle<T> resourceHandle) {

            bool resourceChanged;
            return Render(resourceHandle, resourceChanged);

        }

		ResourceHandle<T> Render(ResourceHandle<T> resourceHandle, bool& resourceChanged) {

            const float padding = 8.0f;

            auto region = ImGui::GetContentRegionAvail();
            auto lineHeight = ImGui::GetTextLineHeight();
            auto deleteButtonSize = ImVec2(lineHeight, lineHeight);
            auto resourceButtonSize = region.x - (resourceHandle.IsValid() ? deleteButtonSize.x + 2.0f * padding : 0.0f);

            resourceChanged = false;
            auto buttonName = resourceHandle.IsValid() ? resourceHandle.GetResource()->GetFileName() :
                "Drop resource here";

            popup.SetID(resourceHandle.GetID());
            if (ImGui::Button(buttonName.c_str(), { resourceButtonSize, 0 }))
                popup.Open();

            // Such that drag and drop will work from the content browser
            if (ImGui::IsDragDropActive() && ImGui::IsItemHovered(ImGuiHoveredFlags_RectOnly)) {
                ImGui::SetWindowFocus();
                ImGui::SetItemDefaultFocus();
            }

            auto handle = ResourcePayloadHelper::AcceptDropResource<T>();
            // Need to change here already
            if (handle.IsValid()) {
                resourceHandle = handle;
                resourceChanged = true;
            }

            if (resourceHandle.IsValid()) {
                ImGui::SameLine();
                
                auto& deleteIcon = Singletons::icons->Get(IconType::Delete);
                auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&deleteIcon);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::ImageButton(set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
                    resourceHandle = ResourceHandle<T>();
                    resourceChanged = true;
                }
                ImGui::PopStyleColor();
            }

            auto resources = ResourceManager<T>::GetResources();
            handle = popup.Render(resources);

            if (handle.IsValid()) {
                resourceHandle = handle;
                resourceChanged = true;
            }

            return resourceHandle;

		}

	private:
		ResourceSelectionPopup popup;

	};

}