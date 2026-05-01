#include <gtest/gtest.h>
#include "Engine.h"
#include "EngineInstance.h"
#include "graphics/Instance.h"
#include "common/Path.h"
#include "App.h"

#include <iomanip>
#include <sstream>
#include <string>

#if defined(AE_OS_ANDROID) || defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
#include <zconf.h>
#endif

#ifdef AE_OS_WINDOWS
#include <direct.h>
#include <Windows.h>
#endif

extern Atlas::EngineInstance* GetEngineInstance();

class EngineEndToEndTest : public testing::TestWithParam<AppConfiguration> {
private:
    static std::string FormatVulkanVersion(uint32_t version) {
        return std::to_string(VK_API_VERSION_MAJOR(version)) + "." +
            std::to_string(VK_API_VERSION_MINOR(version)) + "." +
            std::to_string(VK_API_VERSION_PATCH(version));
    }

    static std::string FormatConformanceVersion(const VkConformanceVersion& version) {
        return std::to_string(version.major) + "." +
            std::to_string(version.minor) + "." +
            std::to_string(version.subminor) + "." +
            std::to_string(version.patch);
    }

    static std::string FormatDeviceType(VkPhysicalDeviceType type) {
        switch (type) {
            case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "Other";
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "Integrated GPU";
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "Discrete GPU";
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "Virtual GPU";
            case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
            default: return "Unknown";
        }
    }

    static std::string FormatUuid(const uint8_t* uuid) {
        std::ostringstream stream;
        stream << std::hex << std::setfill('0');
        for (uint32_t i = 0; i < VK_UUID_SIZE; i++) {
            if (i > 0) {
                stream << ":";
            }
            stream << std::setw(2) << uint32_t(uuid[i]);
        }

        return stream.str();
    }

    static void LogGraphicsDriverInformation(const Atlas::Graphics::GraphicsDevice* graphicsDevice) {
        const auto& properties = graphicsDevice->deviceProperties.properties;
        const auto& driverProperties = graphicsDevice->driverProperties;

        Atlas::Log::Message("Graphics device: " + std::string(properties.deviceName));
        Atlas::Log::Message("Graphics device type: " + FormatDeviceType(properties.deviceType));
        Atlas::Log::Message("Graphics vendor ID: " + std::to_string(properties.vendorID));
        Atlas::Log::Message("Graphics device ID: " + std::to_string(properties.deviceID));
        Atlas::Log::Message("Vulkan API version: " + FormatVulkanVersion(properties.apiVersion));
        Atlas::Log::Message("Vulkan device UUID: " + FormatUuid(graphicsDevice->deviceProperties11.deviceUUID));
        Atlas::Log::Message("Vulkan driver version: " + std::to_string(properties.driverVersion));

        if (driverProperties.driverName[0] != '\0') {
            Atlas::Log::Message("Vulkan driver name: " + std::string(driverProperties.driverName));
            Atlas::Log::Message("Vulkan driver info: " + std::string(driverProperties.driverInfo));
            Atlas::Log::Message("Vulkan driver ID: " + std::to_string(driverProperties.driverID));
            Atlas::Log::Message("Vulkan conformance version: " +
                FormatConformanceVersion(driverProperties.conformanceVersion));
        }
        else {
            Atlas::Log::Message("Vulkan driver properties are not available");
        }
    }

protected:
    void SetUp() override {
        graphicsInstance = Atlas::Graphics::Instance::DefaultInstance;
        graphicsDevice = Atlas::Graphics::GraphicsDevice::DefaultDevice;

        engineInstance = GetEngineInstance();
        ASSERT_NE(engineInstance, nullptr);
    }

    void TearDown() override {
        delete engineInstance;

        graphicsDevice->ForceMemoryCleanup();
    }

    Atlas::Graphics::Instance* graphicsInstance = nullptr;
    Atlas::Graphics::GraphicsDevice* graphicsDevice = nullptr;
    Atlas::EngineInstance* engineInstance = nullptr;

public:
    static void SetUpTestSuite()  {
        Atlas::Engine::Init(Atlas::EngineInstance::engineConfig);

        auto graphicsInstance = Atlas::Graphics::Instance::DefaultInstance;
        if (graphicsInstance->validationLayersEnabled)
            Atlas::Log::Message("Validation layers are set up to be enalbed");
        else
            Atlas::Log::Message("Validation layers are disabled");

        ASSERT_EQ(graphicsInstance->isComplete, true);
        ASSERT_NE(Atlas::Graphics::GraphicsDevice::DefaultDevice, nullptr);
        LogGraphicsDriverInformation(Atlas::Graphics::GraphicsDevice::DefaultDevice);
    }

    static void TearDownTestSuite() {
        Atlas::Engine::Shutdown();
        delete Atlas::Graphics::Instance::DefaultInstance;
    }

};

TEST_P(EngineEndToEndTest, DemoTest) {
    ASSERT_NO_FATAL_FAILURE({

        bool quit = false;
        Atlas::Events::EventManager::QuitEventDelegate.Subscribe(
            [&quit]() {
                quit = true;
            });

        dynamic_cast<App*>(engineInstance)->LoadContent(GetParam());

        while (!quit) {

            Atlas::Engine::Update();

            auto deltaTime = Atlas::Clock::GetDelta();

            engineInstance->Update();

            engineInstance->Update(deltaTime);
            engineInstance->Render(deltaTime);

            graphicsDevice->SubmitFrame();

        }

        engineInstance->UnloadContent();

        });
}

auto testingValues = testing::Values(
    AppConfiguration { .sss = false },
    AppConfiguration { .clouds = false },
    AppConfiguration { .fog = false },
    AppConfiguration { .taa = false },
    AppConfiguration { .ssgi = false },
    AppConfiguration { .ocean = false },
#ifdef AE_BINDLESS
    AppConfiguration { .ddgi = false },
    AppConfiguration { .rtgi = false },
    AppConfiguration { .reflection = false },
#endif
    AppConfiguration { .volumetric = false },
    AppConfiguration { .localVolumetric = false },
    AppConfiguration { .sharpen = false },
    AppConfiguration { .light = false },
    AppConfiguration { .fsr = false },
    AppConfiguration { .recreateSwapchain = true },
    AppConfiguration { .resize = true },
    AppConfiguration { .exampleRenderer = true },
    AppConfiguration { .minimizeWindow = true }
    );

INSTANTIATE_TEST_SUITE_P(DemoTestSuite, EngineEndToEndTest, testingValues);

int main(int argc, char** argv) {

#if defined(AE_OS_MACOS) && defined(AE_BINDLESS)
    setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "2", 1);
#endif

    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
