#pragma once

#include "Panel.h"
#include "Singletons.h"
#include "Notifications.h"
#include "ui/popups/ResourceSelectionPopup.h"
#include "tools/ResourcePayloadHelper.h"
#include "ui/windows/ContentBrowserWindow.h"

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
                "Drop resource here##" + std::to_string(counter++);

            ImGui::PushID(resourceHandle.IsValid() ? int32_t(resourceHandle.GetID()) : counter);

            popup.SetID(resourceHandle.IsValid() ? resourceHandle.GetID() : counter);
            if (ImGui::Button(buttonName.c_str(), { resourceButtonSize, 0 }))
                popup.Open();

            if (resourceHandle.IsValid() && ImGui::BeginPopupContextItem()) {
                if (ImGui::MenuItem("Copy name")) {
                    ImGui::SetClipboardText(buttonName.c_str());
                    Notifications::Push({ .message = "Copied file name to clipboard.", .displayTime = 3.0f });
                }
                if (ImGui::MenuItem("Show in content browser")) {
                    ContentBrowserWindow::contentToShowPath = resourceHandle.GetResource()->path;
                }
                ImGui::EndPopup();
            }

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
                
                auto& deleteIcon = Singletons::icons->Get(IconType::Trash);
                auto set = Singletons::imguiWrapper->GetTextureId(&deleteIcon);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                if (ImGui::ImageButton("Resource delete button", set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f))) {
                    resourceHandle = ResourceHandle<T>();
                    resourceChanged = true;
                }
                ImGui::PopStyleColor();
            }

            /*
            // Here we could decide from where we take the content to display in the popup
            std::vector<Content> content;
            switch(ResourceSelectionPopup::resourceOrigin) {
                case ResourceSelectionPopup::ResourceOriginBits::ContentBrowserBit:
                    resources = Content
                    break;
                case ResourceSelectionPopup::ResourceOriginBits::ResourceManagerBit:
                default:
                    resources = ResourceManager<T>::GetResources();
                    break;
            }
            */

            auto resources = ResourceManager<T>::GetResources();
            handle = popup.Render(resources);

            ImGui::PopID();

            if (handle.IsValid()) {
                resourceHandle = handle;
                resourceChanged = true;
            }

            return resourceHandle;

		}

        void Reset() {

            counter = 0;

        }

	private:
		ResourceSelectionPopup popup;

        int32_t counter = 0;

	};

}