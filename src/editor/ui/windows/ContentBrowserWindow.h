#pragma once

#include "Window.h"
#include "Singletons.h"
#include "resource/ResourceManager.h"
#include "common/Path.h"
#include "ImguiExtension/ImguiWrapper.h"

#include <cctype>

namespace Atlas::Editor::UI {

    class ContentBrowserWindow : public Window {

    public:
        ContentBrowserWindow();

        void Render();

    private:
        template<class T>
        void RenderResourceType() {

            auto resources = ResourceManager<T>::GetResources();

            std::sort(resources.begin(), resources.end(),
                [=](ResourceHandle<T> resource0, ResourceHandle<T> resource1) -> bool {

                    return Common::Path::GetFileName(resource0.GetResource()->path) <
                        Common::Path::GetFileName(resource1.GetResource()->path);

                });

            const float padding = 8.0f;
            const float iconSize = 64.f;

            const float itemSize = iconSize + 2.0f * padding;

            float totalWidth = ImGui::GetContentRegionAvail().x;
            auto columnItemCount = int32_t(totalWidth / itemSize);
            columnItemCount = std::max(columnItemCount, 1);

            ImGui::Columns(columnItemCount, nullptr, false);

            for (size_t i = 0; i < resources.size(); i++) {

                auto& handle = resources[i];
                auto& resource = handle.GetResource();

                auto filename = Common::Path::GetFileName(resource->path);

                ImVec2 buttonSize = ImVec2(iconSize, iconSize);

                ImGui::PushID(resource->path.c_str());

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));

                auto set = Singletons::ImguiWrapper->GetTextureDescriptorSet(fileIcon);

                if (ImGui::ImageButton(set, buttonSize, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), int32_t(padding))) {

                }

                if (ImGui::BeginDragDropSource()) {
                    auto addr = static_cast<const void*>(resource.get());
                    ImGui::SetDragDropPayload(typeid(T).name(), &addr, sizeof(Resource<T>*));
                    ImGui::Text("Drag to entity component");

                    ImGui::EndDragDropSource();
                }

                ImGui::PopStyleColor();

                ImGui::TextWrapped("%s", filename.c_str());

                ImGui::PopID();

                ImGui::NextColumn();

            }

        }

        Texture::Texture2D folderIcon;
        Texture::Texture2D fileIcon;

    };

}