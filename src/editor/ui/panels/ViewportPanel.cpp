#include "ViewportPanel.h"
#include "Singletons.h"
#include "Theme.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    ViewportPanel::ViewportPanel() : Panel("Viewport") {

        viewport = CreateRef<Viewport>();
        viewportTexture = Texture::Texture2D(1, 1, VK_FORMAT_R16G16B16A16_SFLOAT);

        CreateRenderPass();

    }

    void ViewportPanel::DrawMenuBar(std::function<void()> func) {

        drawMenuBarFunc = func;

    }


    void ViewportPanel::DrawOverlay(std::function<void()> func) {

        drawOverlayFunc = func;

    }

    void ViewportPanel::Render(Ref<Scene::Scene> &scene, bool isActiveWindow) {

        ImGui::Begin(GetNameID());

        bool isBlocked = Singletons::blockingOperation->block;

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

        auto pos = ivec2(int32_t(windowPos.x), int32_t(windowPos.y));
        auto size = ivec2(int32_t(region.x), int32_t(region.y));
        RenderScene(scene, pos, size, isActiveWindow);

        if (viewportTexture.IsValid() && viewportTexture.image->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            auto set = Singletons::imguiWrapper->GetTextureId(&viewportTexture);
            ImGui::Image(set, region);
        }

        if (drawOverlayFunc && !isBlocked)
            drawOverlayFunc();

        if (frameColor.w > 0.0f) {
            ImGui::GetWindowDrawList()->AddRect(windowPos, ImVec2(windowPos.x + region.x, windowPos.y + region.y),
                ImGui::GetColorU32(frameColor), 0.0f, 0, 3.0f);
        }

        ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));

        if (drawMenuBarFunc && !isBlocked) {
            isFocused |= ImGui::IsWindowFocused();            

            drawMenuBarFunc(); 
        }

        ImGui::EndChild();

        ImGui::End();

    }

    void ViewportPanel::RenderScene(Ref<Scene::Scene>& scene, ivec2 pos, ivec2 size, bool isActive) { 

        bool validSize = size.x > 0 && size.y > 0;

        if ((viewportTexture.width != size.x ||
            viewportTexture.height != size.y) && validSize) {
            viewport->Set(pos.x, pos.y, size.x, size.y);
            viewportTexture.Resize(size.x, size.y);
            CreateRenderPass();
        }

        if (scene != nullptr && validSize && isActive && !Singletons::blockingOperation->block) {
            auto& config = Singletons::config;
            auto& renderTarget = this->renderTarget ? this->renderTarget : Singletons::renderTarget;

            if (renderTarget->GetWidth() != viewportTexture.width ||
                renderTarget->GetHeight() != viewportTexture.height) {
                renderTarget->Resize(viewportTexture.width, viewportTexture.height);
            }

            Ref<Renderer::PrimitiveBatch> primitiveBatches[] = {
                primitiveBatchWrapper.primitiveBatchDepthTest,
                primitiveBatchWrapper.primitiveBatchNoDepthTest
            };

            // Scenes without ray tracing data (yet) are rasterized
            if (config->pathTrace && allowPathTracing && scene->IsRtDataValid()) {
                // Overlays like the grid, bounding volumes and brushes are visible when path tracing as well
                Singletons::mainRenderer->PathTraceScene(viewport, renderTarget, scene, &viewportTexture, primitiveBatches);
            }
            else {
                Singletons::mainRenderer->RenderScene(viewport, renderTarget, scene,
                    primitiveBatches, &viewportTexture);

                if (visualization != Lit) {
                    float exposure = 1.0f;
                    if (scene->HasMainCamera())
                        exposure = scene->GetMainCamera().exposure;

                    RenderVisualization(exposure);
                }
            }

            primitiveBatchWrapper.Clear();
        }

    }

    void ViewportPanel::RenderGrid(const Scene::Components::CameraComponent& camera) {

        auto location = camera.GetLocation();

        // The spacing grows with the camera height (1, 10, 100, ...), such that the grid stays readable
        auto height = std::max(std::abs(location.y), 1.0f);
        auto spacing = std::pow(10.0f, std::floor(std::log10(height)));

        const int32_t halfLineCount = 50;
        auto extent = spacing * float(halfLineCount);
        auto center = glm::round(vec2(location.x, location.z) / spacing) * spacing;

        const auto minorColor = vec3(0.25f);
        const auto majorColor = vec3(0.45f);
        const auto xAxisColor = vec3(0.85f, 0.25f, 0.25f);
        const auto zAxisColor = vec3(0.25f, 0.45f, 0.95f);

        auto& wrapper = primitiveBatchWrapper;
        for (int32_t i = -halfLineCount; i <= halfLineCount; i++) {
            auto offset = float(i) * spacing;

            // Line parallel to the z axis at this x position
            auto x = center.x + offset;
            auto xIndex = int64_t(std::round(x / spacing));
            auto xColor = std::abs(x) < 0.5f * spacing ? zAxisColor : (xIndex % 10 == 0 ? majorColor : minorColor);
            wrapper.GetBatch(true)->AddLine(vec3(x, 0.0f, center.y - extent), vec3(x, 0.0f, center.y + extent), xColor, xColor);

            // Line parallel to the x axis at this z position
            auto z = center.y + offset;
            auto zIndex = int64_t(std::round(z / spacing));
            auto zColor = std::abs(z) < 0.5f * spacing ? xAxisColor : (zIndex % 10 == 0 ? majorColor : minorColor);
            wrapper.GetBatch(true)->AddLine(vec3(center.x - extent, 0.0f, z), vec3(center.x + extent, 0.0f, z), zColor, zColor);
        }

    }

    float ViewportPanel::GetToolbarGroupWidth(int32_t buttonCount) const {

        auto buttonExtent = ImGui::GetTextLineHeight() * 1.15f + 2.0f * toolbarFramePadding;
        return float(buttonCount) * buttonExtent + float(buttonCount - 1) * toolbarButtonSpacing + 2.0f * toolbarGroupPadding;

    }

    void ViewportPanel::BeginToolbarGroup(float x, int32_t buttonCount) {

        auto groupHeight = ImGui::GetTextLineHeight() * 1.15f + 2.0f * toolbarFramePadding + 2.0f * toolbarGroupPadding;

        auto windowPos = ImGui::GetWindowPos();
        auto drawList = ImGui::GetWindowDrawList();
        auto min = ImVec2(windowPos.x + x, windowPos.y + toolbarMargin);
        auto max = ImVec2(min.x + GetToolbarGroupWidth(buttonCount), min.y + groupHeight);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(Theme::OverlayBackground()), Theme::overlayRounding);
        drawList->AddRect(min, max, ImGui::GetColorU32(Theme::OverlayBorder()), Theme::overlayRounding);
        ImGui::SetCursorPos(ImVec2(x + toolbarGroupPadding, toolbarMargin + toolbarGroupPadding));

    }

    bool ViewportPanel::ToolbarButton(const char* id, IconType iconType, bool selected, bool enabled,
        const char* tooltip, ImVec2 uvMin, ImVec2 uvMax) {

        auto iconSize = ImGui::GetTextLineHeight() * 1.15f;
        auto& icon = Singletons::icons->Get(iconType);
        auto set = Singletons::imguiWrapper->GetTextureId(&icon);

        // Light mode icons are dark, so they would vanish on a solid accent fill, use a tint there
        bool darkMode = Singletons::config->darkMode;
        auto selectedColor = darkMode ? Theme::AccentFill() : Theme::Accent(0.22f);
        auto selectedHoverColor = darkMode ? Theme::AccentFillHovered() : Theme::Accent(0.32f);

        auto hoverColor = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
        ImGui::PushStyleColor(ImGuiCol_Button, selected ? selectedColor : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, selected ? selectedHoverColor : hoverColor);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, selectedColor);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(toolbarFramePadding, toolbarFramePadding));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

        ImGui::BeginDisabled(!enabled);
        bool pressed = ImGui::ImageButton(id, set, ImVec2(iconSize, iconSize), uvMin, uvMax);
        ImGui::EndDisabled();

        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(3);

        ImGui::SetItemTooltip("%s", tooltip);
        ImGui::SameLine(0.0f, toolbarButtonSpacing);

        return pressed && enabled;

    }

    void ViewportPanel::RenderVisualizationPopup() {

        if (!ImGui::BeginPopup("Visualization settings"))
            return;

        auto menuItem = [&](const char* name, ViewportVisualization vis) {
            bool selected = visualization == vis;
            ImGui::MenuItem(name, nullptr, &selected);
            if (selected)
                visualization = vis;
            };

        ImGui::SeparatorText("Visualization");

        menuItem("Lit", ViewportVisualization::Lit);

        if (ImGui::BeginMenu("GBuffer")) {
            menuItem("Base color", ViewportVisualization::GBufferBaseColor);
            menuItem("Roughness/Metalness/Ao", ViewportVisualization::GBufferRoughnessMetalnessAo);
            menuItem("Emissive color", ViewportVisualization::GBufferEmissive);
            menuItem("Depth", ViewportVisualization::GBufferDepth);
            menuItem("Normals", ViewportVisualization::GBufferNormals);
            menuItem("Geometry normals", ViewportVisualization::GBufferGeometryNormals);
            menuItem("Velocity", ViewportVisualization::GBufferVelocity);
            menuItem("Material index", ViewportVisualization::GBufferMaterialIdx);
            menuItem("Stencil", ViewportVisualization::GBufferStencil);
            ImGui::EndMenu();
        }

        menuItem("Volumetrics", ViewportVisualization::Volumetrics);
        menuItem("Clouds", ViewportVisualization::Clouds);
        menuItem("Reflections", ViewportVisualization::Reflections);
        menuItem("SSS", ViewportVisualization::SSS);
        menuItem("SSGI", ViewportVisualization::SSGI);

        ImGui::EndPopup();

    }

    void ViewportPanel::RenderVisualization(float exposure) {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        auto& renderTarget = this->renderTarget ? this->renderTarget : Singletons::renderTarget;
        auto& mainRenderer = Singletons::mainRenderer;

        auto rtData = renderTarget->GetData(Renderer::RenderResolution::FULL_RES);

        auto commandList = graphicsDevice->GetCommandList(Atlas::Graphics::GraphicsQueue);
        commandList->BeginCommands();
        commandList->BeginRenderPass(renderPass, frameBuffer, true);

        if (visualization == GBufferBaseColor) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->baseColorTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        if (visualization == GBufferRoughnessMetalnessAo) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->roughnessMetallicAoTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        if (visualization == GBufferEmissive) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->emissiveTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        if (visualization == GBufferNormals) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->normalTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        if (visualization == GBufferGeometryNormals) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->geometryNormalTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        if (visualization == GBufferMaterialIdx) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->materialIdxTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f / 10000.0f, false, true);
        }
        if (visualization == GBufferStencil) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->stencilTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f / 255.0f, false, true);
        }
        if (visualization == GBufferDepth) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->depthTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        if (visualization == GBufferVelocity) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, rtData->velocityTexture.get(),
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 100.0f, false, true);
        }
        else if (visualization == Reflections) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->reflectionTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, exposure, false, true);
        }
        else if (visualization == Volumetrics) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->volumetricTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, exposure, false, true);
        }
        else if (visualization == Clouds) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->volumetricCloudsTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, exposure, false, true);
        }
        else if (visualization == SSS) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->sssTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        else if (visualization == SSGI) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->giTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, exposure, false, true);
        }

        commandList->EndRenderPass();

        commandList->ImageMemoryBarrier(viewportTexture.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

        commandList->EndCommands();
        graphicsDevice->SubmitCommandList(commandList);

    }

    void ViewportPanel::CreateRenderPass() {

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        auto colorAttachment = Graphics::RenderPassColorAttachment{
            .imageFormat = viewportTexture.image->format,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .outputLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        auto renderPassDesc = Graphics::RenderPassDesc{
            .colorAttachments = { colorAttachment }
        };

        renderPass = device->CreateRenderPass(renderPassDesc);

        auto frameBufferDesc = Graphics::FrameBufferDesc {
            .renderPass = renderPass,
            .colorAttachments = {
                { viewportTexture.image, 0, true },
            },
            .extent = { uint32_t(viewportTexture.width), uint32_t(viewportTexture.height) }
        };
        frameBuffer = device->CreateFrameBuffer(frameBufferDesc);

    }

}