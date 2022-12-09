
#ifndef AE_GRAPHICINSTANCE_H
#define AE_GRAPHICINSTANCE_H

#include <string>
#include <vector>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "Common.h"
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
            Instance(const std::string& instanceName, bool enableValidationLayers = false);

            ~Instance();

            VkInstance GetNativeInstance() const;

            GraphicsDevice* GetGraphicsDevice() const;

            Surface* CreateSurface(SDL_Window* window);

            bool isComplete = false;

        private:
            void LoadSupportedLayersAndExtensions();

            bool CheckExtensionSupport(const std::vector<const char*>& extensionNames);

            bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayerNames);

            void InitializeGraphicsDevice(Surface* surface);

            bool DestroyGraphicsDevice();

            void RegisterDebugCallback();

            VkDebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo();

            bool CheckRequiredVector(const std::vector<const char*>& available, const std::vector<const char*>& required);

            const std::string name;
            bool validationLayersEnabled;

            std::vector<VkLayerProperties> layerProperties;
            std::vector<const char*> layerNames;
            std::vector<VkExtensionProperties> extensionProperties;
            std::vector<const char*> extensionNames;

            VkInstance instance;
            VkDebugUtilsMessengerEXT debugMessenger;

            GraphicsDevice* graphicsDevice;
            std::vector<Surface*> surfaces;

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
