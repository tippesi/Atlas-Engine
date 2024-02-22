#include "ViewportPanel.h"
#include "Singletons.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    ViewportPanel::ViewportPanel() : Panel("Viewport") {

        viewport = CreateRef<Viewport>();
        viewportTexture = Texture::Texture2D(1, 1, VK_FORMAT_R16G16B16A16_SFLOAT);

        primitiveBatchWrapper.primitiveBatch->testDepth = false;

    }

    void ViewportPanel::DrawMenuBar(std::function<void()> func) {

        drawMenuBarFunc = func;

    }


    void ViewportPanel::DrawOverlay(std::function<void()> func) {

        drawOverlayFunc = func;

    }

    void ViewportPanel::Render(Ref<Scene::Scene> &scene, bool isParentFocused) {

        ImGui::Begin(GetNameID());

        // Workaround for offsetted Gizmo without resize after a restart
        // Seems like some ImGuizmo or ImGui config isn't properly updated
        if (firstFrame) {
            auto size = ImGui::GetWindowPos();
            ImGui::SetWindowPos(ImVec2(size.x + 1.0f, size.y));
            firstFrame = false;
        }

        isFocused = ImGui::IsWindowFocused();

        ImGui::BeginChild("Viewport area", ImVec2(0.0f, 0.0f));

        isFocused |= ImGui::IsWindowFocused();

        auto region = ImGui::GetContentRegionAvail();
        auto windowPos = ImGui::GetWindowPos();

        bool validRegion = region.x > 0.0f && region.y > 0.0f;

        if ((viewportTexture.width != int32_t(region.x) ||
            viewportTexture.height != int32_t(region.y)) && validRegion) {
            viewport->Set(int32_t(windowPos.x), int32_t(windowPos.y), int32_t(region.x), int32_t(region.y));
            viewportTexture.Resize(int32_t(region.x), int32_t(region.y));
        }

        if (scene != nullptr && validRegion) {
            auto& config = Singletons::config;

            if (config->pathTrace) {
                auto& pathTraceRenderTarget = Singletons::pathTraceRenderTarget;

                if (pathTraceRenderTarget->GetWidth() != viewportTexture.width ||
                    pathTraceRenderTarget->GetHeight() != viewportTexture.height) {
                    pathTraceRenderTarget->Resize(viewportTexture.width, viewportTexture.height);
                }

                Singletons::mainRenderer->PathTraceScene(viewport, pathTraceRenderTarget, scene, &viewportTexture);
            }
            else {
                auto& renderTarget = Singletons::renderTarget;

                if (renderTarget->GetWidth() != viewportTexture.width ||
                    renderTarget->GetHeight() != viewportTexture.height) {
                    renderTarget->Resize(viewportTexture.width, viewportTexture.height);
                }

                Singletons::mainRenderer->RenderScene(viewport, renderTarget, scene,
                    primitiveBatchWrapper.primitiveBatch, &viewportTexture);
            }

            primitiveBatchWrapper.primitiveBatch->Clear();

        }

        if (viewportTexture.IsValid() && viewportTexture.image->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(viewportTexture);
            ImGui::Image(set, region);
        }

        if (drawOverlayFunc)
            drawOverlayFunc();

        ImGui::EndChild();



        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));

        if (drawMenuBarFunc) {
            ImGui::BeginChild("Viewport area");

            isFocused |= ImGui::IsWindowFocused();            

            drawMenuBarFunc();

            ImGui::EndChild();        
        }


        ImGui::End();

    }

}