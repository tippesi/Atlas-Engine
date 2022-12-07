
#ifndef AE_GRAPHICINSTANCE_H
#define AE_GRAPHICINSTANCE_H

#include <string>
#include <vector>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

namespace Atlas {

    namespace Graphics {

        class Instance {

        public:
            Instance(const std::string& instanceName, bool& success, bool enableValidationLayers = false);

            ~Instance();

        private:
            bool LoadSupportedLayersAndExtensions();

            bool CheckExtensionSupport(std::vector<const char*> extensionNames);

            bool CheckValidationLayerSupport(std::vector<const char*> validationLayerNames);

            bool RegisterDebugCallback();

            VkDebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo();

            bool CheckVectorIntersection(std::vector<const char*>& v1, std::vector<const char*>& v2);

            const std::string name;
            bool validationLayersEnabled;

            std::vector<VkLayerProperties> layerProperties;
            std::vector<const char*> layerNames;
            std::vector<VkExtensionProperties> extensionProperties;
            std::vector<const char*> extensionNames;

            VkInstance instance;
            VkDebugUtilsMessengerEXT debugMessenger;

        private:
            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                    void* pUserData);

        };

    }

}

#endif
