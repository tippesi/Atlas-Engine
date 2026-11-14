#pragma once

#include "Panel.h"
#include "Singletons.h"
#include "Notifications.h"
#include "Theme.h"
#include "ui/Widgets.h"
#include "ui/popups/ResourceSelectionPopup.h"
#include "tools/ResourcePayloadHelper.h"
#include "ui/windows/ContentBrowserWindow.h"

#include <Material.h>
#include <ImguiExtension/UiElements.h>
#include <type_traits>

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

            if constexpr (std::is_same_v<T, Texture::Texture2D>) {
                if (resourceHandle.IsLoaded()) {
                    ImguiExtension::UIElements::TexturePreview(Singletons::imguiWrapper, &resourceHandle);
                    ImGui::SameLine();
                }
            }

            auto region = ImGui::GetContentRegionAvail();
            auto lineHeight = ImGui::GetTextLineHeight();
            auto deleteButtonSize = ImVec2(lineHeight, lineHeight);
            auto resourceButtonSize = region.x - (resourceHandle.IsValid() ? deleteButtonSize.x + 2.0f * padding : 0.0f);

            // Materials and graphs open in their editor from here, like meshes do from their component
            auto canOpen = false;
            if constexpr (std::is_same_v<T, Material> || std::is_same_v<T, MaterialGraph::Graph>)
                canOpen = resourceHandle.IsValid() && !resourceHandle.IsGenerated();
            if (canOpen)
                resourceButtonSize -= deleteButtonSize.x + 2.0f * padding;

            resourceChanged = false;
            auto buttonName = resourceHandle.IsValid() ? resourceHandle.GetResource()->GetFileName() :
                "Drop resource here##" + std::to_string(counter++);

            ImGui::PushID(resourceHandle.IsValid() ? int32_t(resourceHandle.GetID()) : counter);

            popup.SetID(resourceHandle.IsValid() ? resourceHandle.GetID() : counter);
            // Empty slots are muted, all slots are outlined while dragging to show where resources can be dropped
            bool isEmpty = !resourceHandle.IsValid();
            bool dragActive = ImGui::IsDragDropActive();
            if (isEmpty)
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            if (dragActive) {
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
                ImGui::PushStyleColor(ImGuiCol_Border, Theme::Accent(0.8f));
            }

            if (ImGui::Button(buttonName.c_str(), { resourceButtonSize, 0 }))
                popup.Open();

            if (dragActive) {
                ImGui::PopStyleColor();
                ImGui::PopStyleVar();
            }
            if (isEmpty)
                ImGui::PopStyleColor();

            if (!isEmpty)
                ImGui::SetItemTooltip("%s", resourceHandle.GetResource()->path.c_str());

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

            // Buttons next to the resource have the frame padding of the resource button, so they are as high as the row
            auto rowIconButton = [&](const char* id, IconType iconType, const char* tooltip) {
                auto& icon = Singletons::icons->Get(iconType);
                auto set = Singletons::imguiWrapper->GetTextureId(&icon);

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
                auto pressed = ImGui::ImageButton(id, set, deleteButtonSize, ImVec2(0.1f, 0.1f), ImVec2(0.9f, 0.9f));
                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

                ImGui::SetItemTooltip("%s", tooltip);
                return pressed;
                };

            if (canOpen) {
                ImGui::SameLine();
                if (rowIconButton("Resource open button", IconType::OpenInNew, "Open in material editor"))
                    ContentBrowserWindow::contentToEditPath = resourceHandle.GetResource()->path;
            }

            if (resourceHandle.IsValid()) {
                ImGui::SameLine();
                if (rowIconButton("Resource delete button", IconType::Trash, "Remove")) {
                    resourceHandle = ResourceHandle<T>();
                    resourceChanged = true;
                }
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