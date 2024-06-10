#include "Instance.h"
#include "StructureChainBuilder.h"
#include "../Log.h"

#include <volk.h>
#include <set>
#include <iterator>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_macos.h>

namespace Atlas {

    namespace Graphics {

        Instance* Instance::DefaultInstance = nullptr;

        Instance::Instance(const InstanceDesc& desc) :  name(desc.instanceName), 
            validationLayersEnabled(desc.enableValidationLayers), validationLayerSeverity(desc.validationLayerSeverity) {

            VK_CHECK(volkInitialize());

            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = name.c_str();
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "Atlas Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);
            appInfo.apiVersion = VK_API_VERSION_1_2;

            LoadSupportedLayersAndExtensions();

            const std::vector<const char*> validationLayers = {
                    "VK_LAYER_KHRONOS_validation",
            };

            if (validationLayersEnabled && !CheckValidationLayerSupport(validationLayers)) {
                Log::Warning("Required validation layers were not found. Disabling validation layers");
                validationLayersEnabled = false;
            }

            auto requiredExtensions = desc.requiredExtensions;
#ifdef AE_HEADLESS
            AE_ASSERT(supportedExtensions.contains(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) && "Headless instance extension not supported");
            requiredExtensions.push_back(VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME);
#endif
#ifndef AE_BUILDTYPE_RELEASE
            if (supportedExtensions.contains(VK_EXT_DEBUG_UTILS_EXTENSION_NAME) &&
                supportedExtensions.contains(VK_EXT_DEBUG_REPORT_EXTENSION_NAME)) {
                requiredExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
                requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }
#endif
#ifdef AE_OS_MACOS
            if (supportedExtensions.contains(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
                requiredExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            }
#endif
            if (supportedExtensions.contains(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME) && validationLayersEnabled) {
                requiredExtensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
            }

            CheckExtensionSupport(requiredExtensions);

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledLayerCount = 0;
#ifdef AE_OS_MACOS
            if (supportedExtensions.contains(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME)) {
                createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
#endif
            createInfo.enabledExtensionCount = uint32_t(requiredExtensions.size());
            createInfo.ppEnabledExtensionNames = requiredExtensions.data();

            auto debugCreateInfo = GetDebugMessengerCreateInfo();

            // Only enable these with validation layers enabled as well as debug mode enabled
            // They do take away a large chunk of performance
            VkValidationFeatureEnableEXT enables[] = {
                VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
            };
            VkValidationFeaturesEXT validationFeatures = {};

            StructureChainBuilder structureChainBuilder(createInfo);

            if (validationLayersEnabled) {
                createInfo.enabledLayerCount = uint32_t(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();

                structureChainBuilder.Append(debugCreateInfo);

#ifdef AE_BUILDTYPE_DEBUG                
                validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
                validationFeatures.enabledValidationFeatureCount = std::size(enables);
                validationFeatures.pEnabledValidationFeatures = enables;

                structureChainBuilder.Append(validationFeatures);                
#endif
            }
            else {
                createInfo.enabledLayerCount = 0;
            }

            VK_CHECK_MESSAGE(vkCreateInstance(&createInfo, nullptr, &instance), "Error creating instance");

            volkLoadInstance(instance);

            RegisterDebugCallback();
            isComplete = true;

        }

        Instance::~Instance() {

            delete graphicsDevice;

            for (auto surface : surfaces) {
                delete surface;
            }

            if (validationLayersEnabled) {
                vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            }
            vkDestroyInstance(instance, nullptr);

        }

        VkInstance Instance::GetNativeInstance() const {

            return instance;

        }

        GraphicsDevice *Instance::GetGraphicsDevice() const {

            return graphicsDevice;

        }

        Surface *Instance::CreateSurface(SDL_Window* window) {

            bool success = false;
            auto surface = new Surface(this, window, success);
            if (!success) {
                return nullptr;
            }

            surfaces.push_back(surface);

            return surface;

        }

        Surface* Instance::CreateHeadlessSurface() {

            bool success = false;
            auto surface = new Surface(this, success);
            if (!success) {
                return nullptr;
            }

            surfaces.push_back(surface);

            return surface;

        }

        void Instance::LoadSupportedLayersAndExtensions() {

            unsigned int extensionCount = 0;
            bool success = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) == VK_SUCCESS;
            extensionProperties.resize(extensionCount);
            VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data()))

            for (auto& extensionProperty : extensionProperties) {
                // Workaround for issue with Nsight Graphics GPU Profiler
                // This extensions is given back but not supported by the instance
                if (std::string(extensionProperty.extensionName) == "VK_EXT_tooling_info" ||
                    std::string(extensionProperty.extensionName) == "VK_LUNARG_direct_driver_loading")
                    continue;
                extensionNames.push_back(extensionProperty.extensionName);
                supportedExtensions.insert(extensionProperty.extensionName);
            }

            unsigned int layerCount = 0;
            success &= vkEnumerateInstanceLayerProperties(&layerCount, nullptr) == VK_SUCCESS;
            layerProperties.resize(layerCount);
            VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data()));

            for (auto& layerProperty : layerProperties) {
                layerNames.push_back(layerProperty.layerName);
            }

        }

        bool Instance::CheckExtensionSupport(const std::vector<const char*>& extensionNames) {

            std::vector<std::string> availableElements(this->extensionNames.begin(), this->extensionNames.end());
            std::set<std::string> requiredElements(extensionNames.begin(), extensionNames.end());

            for (const auto& element : availableElements) {
                requiredElements.erase(element);
            }

            AE_ASSERT(requiredElements.empty() && "Not all required instance extensions were found");

            return requiredElements.empty();

        }

        bool Instance::CheckValidationLayerSupport(const std::vector<const char*>& validationLayerNames) {

            std::vector<std::string> availableElements(this->layerNames.begin(), this->layerNames.end());
            std::set<std::string> requiredElements(validationLayerNames.begin(), validationLayerNames.end());

            for (const auto& element : availableElements) {
                requiredElements.erase(element);
            }

            return requiredElements.empty();

        }

        void Instance::InitializeGraphicsDevice(Surface* surface) {

            graphicsDevice = new GraphicsDevice(surface, validationLayersEnabled);

        }

        void Instance::RegisterDebugCallback() {

            if (!validationLayersEnabled) return;

            auto createInfo = GetDebugMessengerCreateInfo();
            VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger))

        }

        VkDebugUtilsMessengerCreateInfoEXT Instance::GetDebugMessengerCreateInfo() {

            int32_t allowedSeverity = static_cast<int32_t>(validationLayerSeverity);

            VkDebugUtilsMessengerCreateInfoEXT createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            if (allowedSeverity <= 1) {
                createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
            }
            if (allowedSeverity <= 0) {
                createInfo.messageSeverity |= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
            }

            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT;
            createInfo.pfnUserCallback = DebugCallback;
            createInfo.pUserData = static_cast<void*>(const_cast<char*>(name.c_str()));
            return createInfo;

        }

        VkBool32 Instance::DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData) {

            int32_t logType = Log::Type::TYPE_MESSAGE, logSeverity = Log::Severity::SEVERITY_LOW;

            std::string output = "Vulkan debug log:\n";
            output.append(pCallbackData->pMessage);
            output.append("\nType: ");

            switch (messageType) {
                case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT: output.append("Validation");
                    logType = Log::Type::TYPE_ERROR; break;
                case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT: output.append("Performance");
                    logType = Log::Type::TYPE_WARNING; break;
                case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT: output.append("General");
                    logType = Log::Type::TYPE_MESSAGE;  break;
                default: break;
            }

            output.append("\nSeverity: ");

            switch (messageSeverity) {
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT: output.append("Error");
                    logSeverity = Log::Severity::SEVERITY_HIGH; break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT: output.append("Warning");
                    logSeverity = Log::Severity::SEVERITY_MEDIUM; break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT: output.append("Info");
                    logSeverity = Log::Severity::SEVERITY_LOW; break;
                case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: output.append("Verbose");
                    logSeverity = Log::Severity::SEVERITY_LOW; break;
                default: break;
            }

            // Ignore errors, might happen if the device just provides 4 possible descriptor set locations
            // (This layer uses it's own descriptor set and engine uses 4 already)
            if (pCallbackData->pMessageIdName != nullptr &&
                std::string(pCallbackData->pMessageIdName) == "UNASSIGNED-DEBUG-PRINTF") {
                logType = Log::Type::TYPE_WARNING;
                logSeverity = Log::Severity::SEVERITY_MEDIUM;
            }

            switch (logType) {
                case Log::Type::TYPE_MESSAGE: Log::Message(pCallbackData->pMessage, logSeverity); break;
                case Log::Type::TYPE_WARNING: Log::Warning(pCallbackData->pMessage, logSeverity); break;
                case Log::Type::TYPE_ERROR: Log::Error(pCallbackData->pMessage, logSeverity); break;
            }

#ifndef AE_BUILDTYPE_RELEASE
            if (logSeverity == Log::Severity::SEVERITY_HIGH)
                throw std::runtime_error(output);
#endif

            return VK_TRUE;

        }

    }

}