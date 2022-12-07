
#ifndef AE_GRAPHICINSTANCE_H
#define AE_GRAPHICINSTANCE_H

#include <string>
#include <vector>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "GraphicsDevice.h"
#include "Surface.h"

#include "../Engine.h"

namespace Atlas {

    namespace Graphics {

        class Instance {

            friend GraphicsDevice;
            friend Surface;
            friend Engine;

        public:
            Instance(const std::string& instanceName, bool& success, bool enableValidationLayers = false);

            ~Instance();

            VkInstance GetNativeInstance() const;

            GraphicsDevice* GetGraphicsDevice() const;

        private:
            bool LoadSupportedLayersAndExtensions();

            bool CheckExtensionSupport(std::vector<const char*> extensionNames);

            bool CheckValidationLayerSupport(std::vector<const char*> validationLayerNames);

            bool IntitializeGraphicsDevice(Surface* surface);

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

            GraphicsDevice* graphicsDevice;
            Surface* surface;

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
