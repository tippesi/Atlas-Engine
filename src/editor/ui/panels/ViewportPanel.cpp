#include "ViewportPanel.h"
#include "Singletons.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    ViewportPanel::ViewportPanel() : Panel("Viewport") {

        viewport = CreateRef<Viewport>();
        viewportTexture = Texture::Texture2D(1, 1, VK_FORMAT_R16G16B16A16_SFLOAT);

    }

    void ViewportPanel::Render(Ref<Scene::Scene> &scene) {

        ImGui::Begin(GetNameID());

        auto region = ImGui::GetContentRegionAvail();

        bool validRegion = region.x > 0.0f && region.y > 0.0f;

        if ((viewportTexture.width != int32_t(region.x) ||
            viewportTexture.height != int32_t(region.y)) && validRegion) {
            viewport->Set(0, 0, int32_t(region.x), int32_t(region.y));
            viewportTexture.Resize(int32_t(region.x), int32_t(region.y));
        }

        if (ImGui::IsWindowFocused() && scene != nullptr && validRegion) {
            auto& renderTarget = Singletons::RenderTarget;

            if (renderTarget->GetWidth() != viewportTexture.width ||
                renderTarget->GetHeight() != viewportTexture.height) {
                renderTarget->Resize(viewportTexture.width, viewportTexture.height);
            }

            Singletons::MainRenderer->RenderScene(viewport, renderTarget, scene, nullptr, &viewportTexture);

            auto set = Singletons::ImguiWrapper->GetTextureDescriptorSet(viewportTexture);
            ImGui::Image(set, region);
        }

        ImGui::End();

    }

}