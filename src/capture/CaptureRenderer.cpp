#include "CaptureRenderer.h"

#include "graphics/GraphicsDevice.h"

#include <glm/detail/type_half.hpp>

#include <stdexcept>

namespace Atlas::Capture {

    // Images don't support half floats, the data goes through a float image
    static Common::Image<float> ReadFloatImage(Texture::Texture2D& texture) {

        auto data = texture.GetData<float16>();

        Common::Image<float> image(texture.width, texture.height, 4);
        std::transform(data.begin(), data.end(), image.GetData().begin(), [](float16 value) {

            return glm::detail::toFloat32(value);

        });

        return image;

    }

    void CaptureRenderer::Init(Ref<Renderer::MainRenderer> mainRenderer, ivec2 resolution) {

        this->mainRenderer = mainRenderer;
        this->resolution = resolution;

        renderTarget = CreateRef<Renderer::RenderTarget>(resolution.x, resolution.y);
        viewport = CreateRef<Viewport>(0, 0, resolution.x, resolution.y);

        // Post-processing output is already tone mapped and gamma corrected
        outputTexture = CreateRef<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R16G16B16A16_SFLOAT);

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        // Passes keep negative values, e.g. of the velocity
        auto colorAttachment = Graphics::RenderPassColorAttachment {
            .imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .outputLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };
        passRenderPass = device->CreateRenderPass(Graphics::RenderPassDesc { .colorAttachments = { colorAttachment } });

    }

    void CaptureRenderer::Resize(ivec2 resolution) {

        this->resolution = resolution;
        renderTarget->Resize(resolution.x, resolution.y);
        viewport->Set(0, 0, resolution.x, resolution.y);
        outputTexture = CreateRef<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R16G16B16A16_SFLOAT);
        // Recreated with the new resolution on their next use
        passTargets.clear();

    }

    void CaptureRenderer::Render(const Ref<Scene::Scene>& scene, const std::vector<std::string>& passes,
        std::span<Ref<Renderer::PrimitiveBatch>> primitiveBatches, bool pathTrace) {

        if (pathTrace)
            mainRenderer->PathTraceScene(viewport, renderTarget, scene, outputTexture.get(), primitiveBatches);
        else
            mainRenderer->RenderScene(viewport, renderTarget, scene, primitiveBatches, outputTexture.get());
        hasFrame = true;

        float exposure = scene->HasMainCamera() ? scene->GetMainCamera().exposure : 1.0f;
        for (const auto& pass : passes)
            RenderPass(pass, exposure);

    }

    void CaptureRenderer::Present(const Ref<Font>& font, const std::vector<TextLine>& lines) {

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        auto commandList = device->GetCommandList();

        commandList->BeginCommands();
        device->swapChain->colorClearValue.color = { 0.0f, 0.0f, 0.0f, 1.0f };
        commandList->BeginRenderPass(device->swapChain, true);

        auto extent = device->swapChain->extent;
        auto windowViewport = CreateRef<Viewport>(0, 0, int32_t(extent.width), int32_t(extent.height));
        auto panelHeight = font && !lines.empty() ? GetPanelHeight(font, lines.size()) : 0.0f;

        if (hasFrame && outputTexture->image->layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            // Keep the aspect ratio of the capture in the area above the panel
            auto areaWidth = float(extent.width);
            auto areaHeight = std::max(float(extent.height) - panelHeight, 1.0f);
            auto scale = std::min(areaWidth / float(outputTexture->width), areaHeight / float(outputTexture->height));
            auto width = float(outputTexture->width) * scale;
            auto height = float(outputTexture->height) * scale;
            // The texture renderer places the image from the bottom and needs to flip it
            mainRenderer->textureRenderer.RenderTexture2D(commandList, windowViewport, outputTexture.get(),
                (areaWidth - width) * 0.5f, panelHeight + (areaHeight - height) * 0.5f, width, height, 0.0f, 1.0f,
                false, true);
        }

        if (panelHeight > 0.0f) {
            auto y = float(extent.height) - panelHeight + panelPadding;
            for (const auto& line : lines) {
                mainRenderer->textRenderer.Render(commandList, windowViewport, font, line.text, panelPadding, y, line.color);
                y += font->lineHeight;
            }
        }

        commandList->EndRenderPass();
        commandList->EndCommands();

        device->SubmitCommandList(commandList);

    }

    float CaptureRenderer::GetPanelHeight(const Ref<Font>& font, size_t lineCount) {

        return float(lineCount) * font->lineHeight + 2.0f * panelPadding;

    }

    void CaptureRenderer::SetRenderScale(float renderScale) {

        if (renderTarget->GetScalingFactor() != renderScale)
            renderTarget->SetScalingFactor(renderScale);

    }

    float CaptureRenderer::GetRenderScale() const {

        return renderTarget->GetScalingFactor();

    }

    Ref<Image> CaptureRenderer::ReadOutput() {

        auto floatImage = ReadFloatImage(*outputTexture);
        auto& floatData = floatImage.GetData();

        // The alpha channel isn't meaningful, write opaque images
        for (size_t i = 3; i < floatData.size(); i += 4)
            floatData[i] = 1.0f;

        auto image = CreateImage(outputTexture->width, outputTexture->height);
        image->SetData(floatImage.ConvertData<uint8_t>());

        return image;

    }

    Ref<Image> CaptureRenderer::ReadPass(const std::string& pass) {

        auto& target = GetPassTarget(pass);
        auto floatImage = ReadFloatImage(*target.texture);
        auto& floatData = floatImage.GetData();

        for (size_t i = 0; i < floatData.size(); i += 4) {
            // Signed screen space motion, no motion is gray
            if (pass == "velocity") {
                floatData[i] = 0.5f + 0.5f * floatData[i];
                floatData[i + 1] = 0.5f + 0.5f * floatData[i + 1];
                floatData[i + 2] = 0.5f;
            }
            floatData[i + 3] = 1.0f;
        }

        auto image = CreateImage(target.texture->width, target.texture->height);
        image->SetData(floatImage.ConvertData<uint8_t>());

        return image;

    }

    const std::vector<std::string>& CaptureRenderer::GetPassNames() {

        static const std::vector<std::string> names = { "albedo", "normal", "geometryNormal", "roughness", "emissive",
            "depth", "velocity", "stencil", "materialIdx", "reflection", "volumetric", "clouds", "sss", "gi" };
        return names;

    }

    void CaptureRenderer::RenderPass(const std::string& pass, float exposure) {

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        auto& target = GetPassTarget(pass);
        auto rtData = renderTarget->GetData(Renderer::RenderResolution::FULL_RES);

        Texture::Texture2D* texture = nullptr;
        float scale = 1.0f;
        if (pass == "albedo")
            texture = rtData->baseColorTexture.get();
        else if (pass == "normal")
            texture = rtData->normalTexture.get();
        else if (pass == "geometryNormal")
            texture = rtData->geometryNormalTexture.get();
        else if (pass == "roughness")
            texture = rtData->roughnessMetallicAoTexture.get();
        else if (pass == "emissive")
            texture = rtData->emissiveTexture.get();
        else if (pass == "depth")
            texture = rtData->depthTexture.get();
        else if (pass == "velocity") {
            texture = rtData->velocityTexture.get();
            scale = 100.0f;
        }
        else if (pass == "stencil") {
            texture = rtData->stencilTexture.get();
            scale = 1.0f / 255.0f;
        }
        else if (pass == "materialIdx") {
            texture = rtData->materialIdxTexture.get();
            scale = 1.0f / 10000.0f;
        }
        else if (pass == "reflection") {
            texture = &renderTarget->reflectionTexture;
            scale = exposure;
        }
        else if (pass == "volumetric") {
            texture = &renderTarget->volumetricTexture;
            scale = exposure;
        }
        else if (pass == "clouds") {
            texture = &renderTarget->volumetricCloudsTexture;
            scale = exposure;
        }
        else if (pass == "sss")
            texture = &renderTarget->sssTexture;
        else if (pass == "gi") {
            texture = &renderTarget->giTexture;
            scale = exposure;
        }

        if (!texture)
            return;

        auto commandList = device->GetCommandList(Graphics::GraphicsQueue);
        commandList->BeginCommands();
        commandList->BeginRenderPass(passRenderPass, target.frameBuffer, true);

        mainRenderer->textureRenderer.RenderTexture2D(commandList, viewport, texture, 0.0f, 0.0f,
            float(viewport->width), float(viewport->height), 0.0f, scale, false, true);

        commandList->EndRenderPass();
        commandList->ImageMemoryBarrier(target.texture->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);
        commandList->EndCommands();

        device->SubmitCommandList(commandList);

    }

    CaptureRenderer::PassTarget& CaptureRenderer::GetPassTarget(const std::string& pass) {

        if (std::find(GetPassNames().begin(), GetPassNames().end(), pass) == GetPassNames().end())
            throw std::runtime_error("Unknown pass '" + pass + "'");

        auto it = passTargets.find(pass);
        if (it != passTargets.end())
            return it->second;

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        PassTarget target;
        target.texture = CreateRef<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R16G16B16A16_SFLOAT);
        target.frameBuffer = device->CreateFrameBuffer(Graphics::FrameBufferDesc { .renderPass = passRenderPass,
            .colorAttachments = { { target.texture->image, 0, true } },
            .extent = { uint32_t(resolution.x), uint32_t(resolution.y) } });

        return passTargets[pass] = target;

    }

}
