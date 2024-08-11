#pragma once

#include "Panel.h"
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

            resourceChanged = false;
            auto buttonName = resourceHandle.IsValid() ? resourceHandle.GetResource()->GetFileName() :
                "Drop resource here";

            popup.SetID(resourceHandle.GetID());
            if (ImGui::Button(buttonName.c_str(), { -FLT_MIN, 0 }))
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