#include "DLSSRenderer.h"
#include "graphics/Instance.h"

#include <nvsdk_ngx_helpers_vk.h>

#include <filesystem>

namespace Atlas::Renderer {

    struct DLSSRenderer::Helpers {
        static constexpr const char* projectId = "e71a5590-902c-44c5-a774-a3323a389902";
        static constexpr const char* engineVersion = "0.2.1";

        static const wchar_t* GetDataPath() {

            // NGX writes its logs there
            static const std::wstring path = std::filesystem::temp_directory_path().wstring();
            return path.c_str();

        }

        static const NVSDK_NGX_FeatureCommonInfo* GetFeatureInfo() {

            // The DLSS library is searched next to the executable first, other than in release builds then in the SDK
#ifdef AE_DLSS_RUNTIME_DIR
            static const std::wstring runtimeDir = std::filesystem::path(AE_DLSS_RUNTIME_DIR).wstring();
            static const wchar_t* paths[] = { runtimeDir.c_str() };
            static NVSDK_NGX_FeatureCommonInfo info = { .PathListInfo = { .Path = paths, .Length = 1 } };
#else
            static NVSDK_NGX_FeatureCommonInfo info = {};
#endif
            return &info;

        }

        static NVSDK_NGX_FeatureDiscoveryInfo GetDiscoveryInfo() {

            NVSDK_NGX_FeatureDiscoveryInfo info = {};
            info.SDKVersion = NVSDK_NGX_Version_API;
            info.FeatureID = NVSDK_NGX_Feature_SuperSampling;
            info.Identifier.IdentifierType = NVSDK_NGX_Application_Identifier_Type_Project_Id;
            info.Identifier.v.ProjectDesc = { projectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, engineVersion };
            info.ApplicationDataPath = GetDataPath();
            info.FeatureInfo = GetFeatureInfo();
            return info;

        }

        static std::vector<std::string> GetExtensionNames(NVSDK_NGX_Result result, uint32_t count, const VkExtensionProperties* properties) {

            std::vector<std::string> names;
            if (NVSDK_NGX_FAILED(result))
                return names;

            for (uint32_t i = 0; i < count; i++)
                names.emplace_back(properties[i].extensionName);
            return names;

        }

        static NVSDK_NGX_PerfQuality_Value GetPerfQuality(float scale) {

            // Only selects the default preset, the render size comes from the render target
            if (scale >= 0.99f) return NVSDK_NGX_PerfQuality_Value_DLAA;
            if (scale >= 0.62f) return NVSDK_NGX_PerfQuality_Value_MaxQuality;
            if (scale >= 0.54f) return NVSDK_NGX_PerfQuality_Value_Balanced;
            if (scale >= 0.42f) return NVSDK_NGX_PerfQuality_Value_MaxPerf;
            return NVSDK_NGX_PerfQuality_Value_UltraPerformance;

        }
    };

    DLSSRenderer::DLSSRenderer(Graphics::GraphicsDevice* device) {

        this->device = device;

        auto result = NVSDK_NGX_VULKAN_Init_with_ProjectID(Helpers::projectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, Helpers::engineVersion,
            Helpers::GetDataPath(), device->instance->GetNativeInstance(), device->physicalDevice, device->device,
            vkGetInstanceProcAddr, vkGetDeviceProcAddr, Helpers::GetFeatureInfo());
        if (NVSDK_NGX_FAILED(result)) {
            Log::Message("DLSS isn't available on this device (NGX result " + std::to_string(int32_t(result)) + ")");
            return;
        }
        initialized = true;

        int32_t available = 0, needsUpdatedDriver = 0;
        result = NVSDK_NGX_VULKAN_GetCapabilityParameters(&parameters);
        if (NVSDK_NGX_SUCCEED(result)) {
            NVSDK_NGX_Parameter_GetI(parameters, NVSDK_NGX_Parameter_SuperSampling_Available, &available);
            NVSDK_NGX_Parameter_GetI(parameters, NVSDK_NGX_Parameter_SuperSampling_NeedsUpdatedDriver, &needsUpdatedDriver);
        }

        supported = available != 0;
        if (supported)
            Log::Message("DLSS is available");
        else if (needsUpdatedDriver)
            Log::Warning("DLSS needs a newer driver");
        else
            Log::Message("DLSS isn't available on this device");

    }

    DLSSRenderer::~DLSSRenderer() {

        if (!initialized)
            return;

        DLSSRenderer::DestroyContext();
        if (parameters)
            NVSDK_NGX_VULKAN_DestroyParameters(parameters);
        NVSDK_NGX_VULKAN_Shutdown1(device->device);

    }

    void DLSSRenderer::Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList) {

        GPUProfiler::Scope gpuScope("DLSS");

        auto renderSize = ivec2(target->GetScaledWidth(), target->GetScaledHeight());

        auto& camera = scene->renderState.camera;
        auto targetData = target->GetData(FULL_RES);

        auto colorImage = target->lightingTexture.image;
        auto depthImage = targetData->depthTexture->image;
        auto velocityImage = target->GetVelocity()->image;
        auto outputImage = target->hdrTexture.image;

        Graphics::ImageBarrier imageBarriers[] = {
            {colorImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            {depthImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            {velocityImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT},
            {outputImage, VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT},
        };
        commandList->PipelineBarrier(imageBarriers, {}, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

        auto getResource = [](const Ref<Graphics::Image>& image, bool readWrite) {
            VkImageSubresourceRange range = { image->aspectFlags, 0, 1, 0, 1 };
            return NVSDK_NGX_Create_ImageView_Resource_VK(image->view, image->image, range, image->format,
                image->width, image->height, readWrite);
        };

        auto color = getResource(colorImage, false);
        auto depth = getResource(depthImage, false);
        auto velocity = getResource(velocityImage, false);
        auto output = getResource(outputImage, true);

        NVSDK_NGX_VK_DLSS_Eval_Params evalParams = {};
        evalParams.Feature.pInColor = &color;
        evalParams.Feature.pInOutput = &output;
        evalParams.pInDepth = &depth;
        evalParams.pInMotionVectors = &velocity;
        // Same conventions as for FSR2: jitter in render pixels, motion vectors in UV space towards the last frame
        evalParams.InJitterOffsetX = camera.GetJitter().x * float(renderSize.x) * 0.5f;
        evalParams.InJitterOffsetY = camera.GetJitter().y * float(renderSize.y) * 0.5f;
        evalParams.InMVScaleX = float(renderSize.x);
        evalParams.InMVScaleY = float(renderSize.y);
        evalParams.InRenderSubrectDimensions = { uint32_t(renderSize.x), uint32_t(renderSize.y) };
        evalParams.InReset = target->HasHistory() ? 0 : 1;
        evalParams.InFrameTimeDeltaInMsec = scene->renderState.deltaTime * 1000.0f;

        auto result = NGX_VULKAN_EVALUATE_DLSS_EXT(commandList->commandBuffer, feature, parameters, &evalParams);
        commandList->InvalidateBoundState();
        if (NVSDK_NGX_FAILED(result)) {
            Log::Warning("DLSS evaluation failed (NGX result " + std::to_string(int32_t(result)) + "), using FSR 2 instead");
            MarkContextFailed();
        }

        commandList->ImageMemoryBarrier(outputImage, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_ACCESS_SHADER_READ_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    }

    std::vector<std::string> DLSSRenderer::GetInstanceExtensions() {

        auto discoveryInfo = Helpers::GetDiscoveryInfo();
        uint32_t count = 0;
        VkExtensionProperties* properties = nullptr;
        auto result = NVSDK_NGX_VULKAN_GetFeatureInstanceExtensionRequirements(&discoveryInfo, &count, &properties);
        return Helpers::GetExtensionNames(result, count, properties);

    }

    std::vector<std::string> DLSSRenderer::GetDeviceExtensions(VkInstance instance, VkPhysicalDevice physicalDevice) {

        auto discoveryInfo = Helpers::GetDiscoveryInfo();
        uint32_t count = 0;
        VkExtensionProperties* properties = nullptr;
        auto result = NVSDK_NGX_VULKAN_GetFeatureDeviceExtensionRequirements(instance, physicalDevice,
            &discoveryInfo, &count, &properties);
        return Helpers::GetExtensionNames(result, count, properties);

    }

    bool DLSSRenderer::CreateContext(ivec2 renderSize, ivec2 displaySize, Graphics::CommandList* commandList) {

        NVSDK_NGX_DLSS_Create_Params createParams = {};
        createParams.Feature.InWidth = uint32_t(renderSize.x);
        createParams.Feature.InHeight = uint32_t(renderSize.y);
        createParams.Feature.InTargetWidth = uint32_t(displaySize.x);
        createParams.Feature.InTargetHeight = uint32_t(displaySize.y);
        createParams.Feature.InPerfQualityValue = Helpers::GetPerfQuality(float(renderSize.x) / float(displaySize.x));
        // The lighting is linear HDR, the exposure is only known after the upscaling
        createParams.InFeatureCreateFlags = NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes |
            NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;

        auto result = NGX_VULKAN_CREATE_DLSS_EXT1(device->device, commandList->commandBuffer, 1, 1,
            &feature, parameters, &createParams);
        commandList->InvalidateBoundState();

        if (NVSDK_NGX_FAILED(result)) {
            Log::Warning("Couldn't create the DLSS feature for " + std::to_string(renderSize.x) + "x" + std::to_string(renderSize.y) +
                " (NGX result " + std::to_string(int32_t(result)) + "), using FSR 2 instead");
            feature = nullptr;
            return false;
        }

        return true;

    }

    void DLSSRenderer::DestroyContext() {

        if (!feature)
            return;

        // The feature may still be used by command lists in flight
        device->WaitForIdle();
        NVSDK_NGX_VULKAN_ReleaseFeature(feature);
        feature = nullptr;

    }

}
