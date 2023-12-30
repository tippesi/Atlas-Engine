#include <gtest/gtest.h>
#include "Engine.h"
#include "EngineInstance.h"
#include "graphics/Instance.h"
#include "common/Path.h"

#if defined(AE_OS_ANDROID) || defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
#include <zconf.h>
#endif

#ifdef AE_OS_WINDOWS
#include <direct.h>
#include <Windows.h>
#endif

// The fixture for testing class Foo.
class EngineEndToEndTest : public testing::Test {
protected:
    // You can remove any or all of the following functions if their bodies would
    // be empty.

    EngineEndToEndTest() {
        // You can do set-up work for each test here.
    }

    ~EngineEndToEndTest() override {
        // You can do clean-up work that doesn't throw exceptions here.
    }

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    void SetUp() override {
        // Code here will be called immediately after the constructor (right
        // before each test).
    }

    void TearDown() override {
        // Code here will be called immediately after each test (right
        // before the destructor).
    }

public:
    // Class members declared here can be used by all tests in the test suite
    // for Foo.
    int test;

};

TEST(EngineEndToEndTest, DemoTest) {

    ASSERT_NO_FATAL_FAILURE({

        Atlas::Engine::Init(Atlas::EngineInstance::engineConfig);

        auto graphicsInstance = Atlas::Graphics::Instance::DefaultInstance;

        if (!graphicsInstance->isComplete) {
            Atlas::Engine::Shutdown();
        }

        auto engineInstance = Atlas::EngineInstance::GetInstance();
        if (!engineInstance) {
            Atlas::Engine::Shutdown();
        }

        auto graphicsDevice = graphicsInstance->GetGraphicsDevice();

        bool quit = false;

        Atlas::Events::EventManager::QuitEventDelegate.Subscribe(
            [&quit]() {
                quit = true;
            });

        engineInstance->LoadContent();

        while (!quit) {

            Atlas::Engine::Update();

            auto deltaTime = Atlas::Clock::GetDelta();

            engineInstance->Update();

            engineInstance->Update(deltaTime);
            engineInstance->Render(deltaTime);

            graphicsDevice->CompleteFrame();

        }

        engineInstance->UnloadContent();
        delete engineInstance;

        Atlas::Engine::Shutdown();
        delete graphicsInstance;
        });

}

int main(int argc, char** argv) {
#ifdef AO_OS_WINDOWS
    SetEnvironmentVariable("VK_LOADER_LAYERS_ENABLE", "VkLayer_khronos_validation");
    SetEnvironmentVariable("VK_ADD_LAYER_PATH", "D:\\a\\Atlas-Engine\\Atlas-Engine\\VULKAN_SDK\\Bin");
    SetEnvironmentVariable("VK_LAYER_PATH", "D:\\a\\Atlas-Engine\\Atlas-Engine\\VULKAN_SDK\\Bin");
#endif
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
