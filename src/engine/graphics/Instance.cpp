#include "Instance.h"
#include "../Log.h"

#include <volk.h>

namespace Atlas {

    namespace Graphics {

        Instance* Instance::defaultInstance = nullptr;

        Instance::Instance(const std::string &instanceName, bool enableValidationLayers) :
            name(instanceName), validationLayersEnabled(enableValidationLayers) {

            VK_CHECK(volkInitialize());

            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = instanceName.c_str();
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "Atlas Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_2;

            LoadSupportedLayersAndExtensions();

            auto requiredExtensions = extensionNames;
#ifdef AE_OS_MACOS
            requiredExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif
            requiredExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

            const std::vector<const char*> validationLayers = {
                    "VK_LAYER_KHRONOS_validation"
            };

            if (!CheckValidationLayerSupport(validationLayers) && enableValidationLayers) {
                return;
            }

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledLayerCount = 0;
#ifdef AE_OS_MACOS
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif
            createInfo.enabledExtensionCount = uint32_t(requiredExtensions.size());
            createInfo.ppEnabledExtensionNames = requiredExtensions.data();

            auto debugCreateInfo = GetDebugMessengerCreateInfo();
            if (enableValidationLayers) {
                createInfo.enabledLayerCount = uint32_t(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();

                createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
            }
            else {
                createInfo.enabledLayerCount = 0;
            }

            VK_CHECK(vkCreateInstance(&createInfo, nullptr, &instance));

            volkLoadInstance(instance);

            RegisterDebugCallback();
            isComplete = true;

        }

        Instance::~Instance() {

            delete graphicsDevice;

            for (auto surface : surfaces) {
                delete surface;
            }

            vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
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
            auto surface = new Surface(window, success);
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
                extensionNames.push_back(extensionProperty.extensionName);
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

            return CheckRequiredVector(this->extensionNames, extensionNames);

        }

        bool Instance::CheckValidationLayerSupport(const std::vector<const char*>& validationLayerNames) {

            return CheckRequiredVector(this->layerNames, validationLayerNames);

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

            VkDebugUtilsMessengerCreateInfoEXT createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = DebugCallback;
            createInfo.pUserData = static_cast<void*>(const_cast<char*>(name.c_str()));
            return createInfo;

        }

        bool Instance::CheckRequiredVector(const std::vector<const char *> &available,
                                           const std::vector<const char *> &required) {

            std::vector<std::string> availableElements(available.begin(), available.end());
            std::set<std::string> requiredElements(required.begin(), required.end());

            for (const auto& element : availableElements) {
                requiredElements.erase(element);
            }
            assert(requiredElements.empty() && "Not all required elements were found");

            return requiredElements.empty();

        }

        VkBool32 Instance::DebugCallback(
                VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                VkDebugUtilsMessageTypeFlagsEXT messageType,
                const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                void *pUserData) {

            int32_t logType = Log::Type::TYPE_MESSAGE, logSeverity = Log::Severity::SEVERITY_LOW;

            // Filter notifications
            //if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
            //    return VK_FALSE;

            // We could use the userParam
            // auto context = static_cast<const Context*>(userParam);

            //std::string output = "OpenGL debug log:\nContext name: "
            //                     + context->name + "\nGenerated by: ";

            std::string output = "Vulkan debug log:";
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

            switch (logType) {
                case Log::Type::TYPE_MESSAGE: Log::Message(pCallbackData->pMessage, logSeverity); break;
                case Log::Type::TYPE_WARNING: Log::Warning(pCallbackData->pMessage, logSeverity); break;
                case Log::Type::TYPE_ERROR: Log::Error(pCallbackData->pMessage, logSeverity); break;
            }

            return VK_TRUE;

        }

    }

}