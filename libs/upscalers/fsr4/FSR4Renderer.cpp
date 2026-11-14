#include "FSR4Renderer.h"

#include <api/include/vk/ffx_api_vk.h>
#include <upscalers/include/ffx_upscale.h>

#include <filesystem>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Atlas::Renderer {

    struct FSR4Renderer::Functions {
        PfnFfxCreateContext createContext = nullptr;
        PfnFfxDestroyContext destroyContext = nullptr;
        PfnFfxQuery query = nullptr;
        PfnFfxDispatch dispatch = nullptr;
    };

    struct FSR4Renderer::Helpers {
        static constexpr const wchar_t* libraryName = L"amd_fidelityfx_upscaler_vk.dll";

        static HMODULE LoadUpscalerLibrary() {

            // The library is searched next to the executable first, other than in release builds then in the SDK
            wchar_t executablePath[MAX_PATH];
            GetModuleFileNameW(nullptr, executablePath, MAX_PATH);
            auto path = std::filesystem::path(executablePath).parent_path() / libraryName;
#ifdef AE_FSR4_RUNTIME_DIR
            if (!std::filesystem::exists(path))
                path = std::filesystem::path(AE_FSR4_RUNTIME_DIR) / libraryName;
#endif

            return LoadLibraryW(path.c_str());

        }

        static void LogMessage(uint32_t type, const wchar_t* message) {

            auto text = "FSR: " + std::filesystem::path(message).string();
            if (type == FFX_API_MESSAGE_TYPE_ERROR)
                Log::Error(text);
            else
                Log::Warning(text);

        }

        static FfxApiResource GetResource(const Ref<Graphics::Image>& image, uint32_t usage, uint32_t state) {

            FfxApiResourceDescription description = {};
            description.type = FFX_API_RESOURCE_TYPE_TEXTURE2D;
            description.format = ffxApiGetSurfaceFormatVK(image->format);
            description.width = image->width;
            description.height = image->height;
            description.depth = 1;
            description.mipCount = 1;
            description.usage = usage;

            return ffxApiGetResourceVK(reinterpret_cast<void*>(image->image), description, state);

        }
    };

    FSR4Renderer::FSR4Renderer(Graphics::GraphicsDevice* device) : functions(std::make_unique<Functions>()) {

        this->device = device;

        auto module = Helpers::LoadUpscalerLibrary();
        if (!module) {
            Log::Message("FSR 4 isn't available, " + std::filesystem::path(Helpers::libraryName).string() + " wasn't found");
            return;
        }
        library = module;

        functions->createContext = reinterpret_cast<PfnFfxCreateContext>(GetProcAddress(module, "ffxCreateContext"));
        functions->destroyContext = reinterpret_cast<PfnFfxDestroyContext>(GetProcAddress(module, "ffxDestroyContext"));
        functions->query = reinterpret_cast<PfnFfxQuery>(GetProcAddress(module, "ffxQuery"));
        functions->dispatch = reinterpret_cast<PfnFfxDispatch>(GetProcAddress(module, "ffxDispatch"));

        VkDeviceContext deviceContext = { device->device, device->physicalDevice, vkGetDeviceProcAddr };

        uint64_t versionCount = 0;
        ffxQueryDescGetVersions versionQuery = {};
        versionQuery.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
        versionQuery.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
        versionQuery.device = &deviceContext;
        versionQuery.outputCount = &versionCount;
        functions->query(nullptr, &versionQuery.header);

        std::vector<const char*> versionNames(versionCount);
        versionQuery.versionNames = versionNames.data();
        functions->query(nullptr, &versionQuery.header);

        std::string versions;
        for (auto name : versionNames)
            versions += (versions.empty() ? "" : ", ") + std::string(name);

        supported = versionCount > 0;
        if (supported)
            Log::Message("FSR upscaler versions available on this device: " + versions);
        else
            Log::Message("The FSR upscaler isn't available on this device");

    }

    FSR4Renderer::~FSR4Renderer() {

        FSR4Renderer::DestroyContext();
        if (library)
            FreeLibrary(static_cast<HMODULE>(library));

    }

    void FSR4Renderer::Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) {

        GPUProfiler::Scope gpuScope("FSR4");

        auto renderSize = ivec2(target->GetScaledWidth(), target->GetScaledHeight());
        auto displaySize = ivec2(target->GetWidth(), target->GetHeight());

        auto& postProcessing = scene->renderState.postProcessing;
        auto& camera = scene->renderState.camera;
        auto targetData = target->GetData(FULL_RES);

        auto colorImage = target->lightingTexture.image;
        auto depthImage = targetData->depthTexture->image;
        auto velocityImage = target->GetVelocity()->image;
        auto reactiveMaskImage = target->reactiveMaskTexture.image;
        auto outputImage = target->hdrTexture.image;

        bool reactiveMask = !target->IsUsedForPathTracing();

        std::vector<Graphics::ImageBarrier> imageBarriers = {
            {colorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            {depthImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            {velocityImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            {outputImage, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
        };
        if (reactiveMask)
            imageBarriers.push_back({reactiveMaskImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT});
        commandList->PipelineBarrier(imageBarriers, {}, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        ffxDispatchDescUpscale dispatch = {};
        dispatch.header.type = FFX_API_DISPATCH_DESC_TYPE_UPSCALE;
        dispatch.commandList = commandList->commandBuffer;
        dispatch.color = Helpers::GetResource(colorImage, FFX_API_RESOURCE_USAGE_READ_ONLY, FFX_API_RESOURCE_STATE_COMPUTE_READ);
        dispatch.depth = Helpers::GetResource(depthImage, FFX_API_RESOURCE_USAGE_DEPTHTARGET, FFX_API_RESOURCE_STATE_COMPUTE_READ);
        dispatch.motionVectors = Helpers::GetResource(velocityImage, FFX_API_RESOURCE_USAGE_READ_ONLY, FFX_API_RESOURCE_STATE_COMPUTE_READ);
        if (reactiveMask)
            dispatch.reactive = Helpers::GetResource(reactiveMaskImage, FFX_API_RESOURCE_USAGE_READ_ONLY, FFX_API_RESOURCE_STATE_COMPUTE_READ);
        dispatch.output = Helpers::GetResource(outputImage, FFX_API_RESOURCE_USAGE_UAV, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
        // Same conventions as for FSR2: jitter in render pixels, motion vectors in UV space towards the last frame
        dispatch.jitterOffset = { camera.GetJitter().x * float(renderSize.x) * 0.5f, camera.GetJitter().y * float(renderSize.y) * 0.5f };
        dispatch.motionVectorScale = { float(renderSize.x), float(renderSize.y) };
        dispatch.renderSize = { uint32_t(renderSize.x), uint32_t(renderSize.y) };
        dispatch.upscaleSize = { uint32_t(displaySize.x), uint32_t(displaySize.y) };
        dispatch.enableSharpening = postProcessing.sharpen.enable;
        dispatch.sharpness = postProcessing.sharpen.factor;
        dispatch.frameTimeDelta = scene->renderState.deltaTime * 1000.0f;
        dispatch.preExposure = scene->renderState.preExposure;
        dispatch.reset = !target->HasHistory();
        dispatch.cameraNear = camera.nearPlane;
        dispatch.cameraFar = camera.farPlane;
        dispatch.cameraFovAngleVertical = glm::radians(camera.fieldOfView);
        dispatch.viewSpaceToMetersFactor = 1.0f;

        auto result = functions->dispatch(&context, &dispatch.header);
        commandList->InvalidateBoundState();
        if (result != FFX_API_RETURN_OK) {
            Log::Warning("The FSR dispatch failed (result " + std::to_string(result) + "), using FSR 2 instead");
            MarkContextFailed();
        }

        commandList->ImageMemoryBarrier(outputImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    }

    std::vector<std::string> FSR4Renderer::GetDeviceExtensions() {

        // The machine learning upscaler runs with cooperative matrices, on RDNA 4 with 8 bit floats
        return { VK_KHR_COOPERATIVE_MATRIX_EXTENSION_NAME, VK_EXT_SHADER_FLOAT8_EXTENSION_NAME };

    }

    bool FSR4Renderer::CreateContext(ivec2 renderSize, ivec2 displaySize, Graphics::CommandList* commandList) {

        ffxCreateBackendVKDesc backendDesc = {};
        backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_VK;
        backendDesc.deviceContext = { device->device, device->physicalDevice, vkGetDeviceProcAddr };

        ffxCreateContextDescUpscale createDesc = {};
        createDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
        createDesc.header.pNext = &backendDesc.header;
        // The lighting is linear HDR, the exposure is only known after the upscaling
        createDesc.flags = FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE | FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
        createDesc.maxRenderSize = { uint32_t(renderSize.x), uint32_t(renderSize.y) };
        createDesc.maxUpscaleSize = { uint32_t(displaySize.x), uint32_t(displaySize.y) };
        createDesc.fpMessage = Helpers::LogMessage;

        auto result = functions->createContext(&context, &createDesc.header, nullptr);
        if (result != FFX_API_RETURN_OK) {
            Log::Warning("Couldn't create the FSR upscaler context for " + std::to_string(renderSize.x) + "x" + std::to_string(renderSize.y) +
                " (result " + std::to_string(result) + "), using FSR 2 instead");
            context = nullptr;
            return false;
        }

        ffxQueryGetProviderVersion versionQuery = {};
        versionQuery.header.type = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
        functions->query(&context, &versionQuery.header);
        if (versionQuery.versionName)
            Log::Message("Upscaling with FSR " + std::string(versionQuery.versionName));

        return true;

    }

    void FSR4Renderer::DestroyContext() {

        if (!context)
            return;

        // The context may still be used by command lists in flight
        device->WaitForIdle();
        functions->destroyContext(&context, nullptr);
        context = nullptr;

    }

}
