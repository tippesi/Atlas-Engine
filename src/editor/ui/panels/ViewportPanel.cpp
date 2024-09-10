#include "ViewportPanel.h"
#include "Singletons.h"

#include <imgui.h>

namespace Atlas::Editor::UI {

    ViewportPanel::ViewportPanel() : Panel("Viewport") {

        viewport = CreateRef<Viewport>();
        viewportTexture = Texture::Texture2D(1, 1, VK_FORMAT_R16G16B16A16_SFLOAT);

        primitiveBatchWrapper.primitiveBatch->testDepth = false;

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
            auto set = Singletons::imguiWrapper->GetTextureDescriptorSet(&viewportTexture);
            ImGui::Image(set, region);
        }

        if (drawOverlayFunc && !isBlocked)
            drawOverlayFunc();

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
            auto& renderTarget = Singletons::renderTarget;

            if (renderTarget->GetWidth() != viewportTexture.width ||
                renderTarget->GetHeight() != viewportTexture.height) {
                renderTarget->Resize(viewportTexture.width, viewportTexture.height);
            }

            if (config->pathTrace) {               
                Singletons::mainRenderer->PathTraceScene(viewport, renderTarget, scene, &viewportTexture);
            }
            else {              
                Singletons::mainRenderer->RenderScene(viewport, renderTarget, scene,
                    primitiveBatchWrapper.primitiveBatch, &viewportTexture);

                if (visualization != Lit) {
                    RenderVisualization();
                }
            }

            primitiveBatchWrapper.primitiveBatch->Clear();
        }

    }

    void ViewportPanel::RenderVisualization() {

        auto graphicsDevice = Graphics::GraphicsDevice::DefaultDevice;

        auto& renderTarget = Singletons::renderTarget;
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
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        else if (visualization == Volumetrics) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->volumetricTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        else if (visualization == Clouds) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->volumetricCloudsTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        else if (visualization == SSS) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->sssTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
        }
        else if (visualization == SSGI) {
            mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, &renderTarget->giTexture,
                0.0f, 0.0f, float(viewport->width), float(viewport->height), 0.0, 1.0f, false, true);
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