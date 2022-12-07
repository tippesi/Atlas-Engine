#include "Instance.h"
#include "../Log.h"

#include <volk.h>

namespace Atlas {

    namespace Graphics {

        Instance::Instance(const std::string &instanceName, bool &success, bool enableValidationLayers) :
            name(instanceName), validationLayersEnabled(enableValidationLayers) {

            success = volkInitialize() == VK_SUCCESS;
            assert(success && "Error initializing Volk");

            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = instanceName.c_str();
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "Atlas Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = VK_API_VERSION_1_0;

            success &= LoadSupportedLayersAndExtensions();

            auto requiredExtensions = extensionNames;
            requiredExtensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
            requiredExtensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

            const std::vector<const char*> validationLayers = {
                    "VK_LAYER_KHRONOS_validation"
            };

            if (!CheckValidationLayerSupport(validationLayers) && enableValidationLayers) {
                success = false;
                return;
            }

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledLayerCount = 0;
            createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
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

            success &= vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS;
            assert(success && "Error creating Vulkan instance");

            volkLoadInstance(instance);

            RegisterDebugCallback();

        }

        Instance::~Instance() {

            vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            vkDestroyInstance(instance, nullptr);

        }

        GraphicsDevice *Instance::CreateGraphicsDevice() {

            bool success = false;
            auto device = new GraphicsDevice(this, success, validationLayersEnabled);

            if (!success) {
                delete device;
                return nullptr;
            }

            return device;

        }

        bool Instance::LoadSupportedLayersAndExtensions() {

            unsigned int extensionCount = 0;
            bool success = vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr) == VK_SUCCESS;
            extensionProperties.resize(extensionCount);
            success &= vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensionProperties.data()) == VK_SUCCESS;
            assert(success && "Error loading extension properties");

            for (auto& extensionProperty : extensionProperties) {
                extensionNames.push_back(extensionProperty.extensionName);
            }

            unsigned int layerCount = 0;
            success &= vkEnumerateInstanceLayerProperties(&layerCount, nullptr) == VK_SUCCESS;
            layerProperties.resize(layerCount);
            success &= vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data()) == VK_SUCCESS;
            assert(success && "Error loading layer properties");

            for (auto& layerProperty : layerProperties) {
                layerNames.push_back(layerProperty.layerName);
            }

            return success;

        }

        bool Instance::CheckExtensionSupport(std::vector<const char*> extensionNames) {

            return CheckVectorIntersection(extensionNames, this->extensionNames);

        }

        bool Instance::CheckValidationLayerSupport(std::vector<const char*> validationLayerNames) {

            return CheckVectorIntersection(validationLayerNames, this->layerNames);

        }

        bool Instance::RegisterDebugCallback() {

            if (!validationLayersEnabled) return true;

            auto createInfo = GetDebugMessengerCreateInfo();
            return vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) == VK_SUCCESS;

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
            createInfo.pUserData = (void*)name.c_str();
            return createInfo;

        }

        bool Instance::CheckVectorIntersection(std::vector<const char *> &v1, std::vector<const char *> &v2) {

            for (const auto a : v1) {
                bool found = false;

                for (const auto b : v2) {
                    if (strcmp(a, b) == 0) {
                        found = true;
                        break;
                    }
                }

                if (!found) return false;
            }
            return true;

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