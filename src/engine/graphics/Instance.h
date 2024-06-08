#pragma once

#include <string>
#include <vector>
#include <set>

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include "Common.h"
#include "GraphicsDevice.h"
#include "Surface.h"
#include "Extensions.h"

#include "../Engine.h"

namespace Atlas {

    namespace Graphics {

        struct InstanceDesc {
            std::string instanceName;

            bool enableValidationLayers = false;
            Log::Severity validationLayerSeverity = Log::Severity::SEVERITY_LOW;

            std::vector<const char*> requiredExtensions;
        };

        class Instance {

            friend GraphicsDevice;
            friend Surface;
            friend Engine;
            friend Extensions;

        public:
            explicit Instance(const InstanceDesc& desc);

            Instance(const Instance& that) = delete;

            ~Instance();

            Instance& operator=(const Instance& that) = delete;

            VkInstance GetNativeInstance() const;

            GraphicsDevice* GetGraphicsDevice() const;

            Surface* CreateSurface(SDL_Window* window);

            Surface* CreateHeadlessSurface();

            bool isComplete = false;

            static Instance* DefaultInstance;

        private:
            void LoadSupportedLayersAndExtensions();

            bool CheckExtensionSupport(const std::vector<const char*>& extensionNames);

            bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayerNames);

            void InitializeGraphicsDevice(Surface* surface);

            bool DestroyGraphicsDevice();

            void RegisterDebugCallback();

            VkDebugUtilsMessengerCreateInfoEXT GetDebugMessengerCreateInfo();

            const std::string name;
            bool validationLayersEnabled;

            std::vector<VkLayerProperties> layerProperties;
            std::vector<const char*> layerNames;
            std::vector<VkExtensionProperties> extensionProperties;
            std::vector<const char*> extensionNames;

            std::set<std::string> supportedExtensions;

            VkInstance instance = {};
            VkDebugUtilsMessengerEXT debugMessenger;

            GraphicsDevice* graphicsDevice;
            std::vector<Surface*> surfaces;

            Log::Severity validationLayerSeverity;

        private:
            static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
                    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                    void* pUserData);

        };

    }

}