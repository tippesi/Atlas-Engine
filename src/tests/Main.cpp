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

    // Class members declared here can be used by all tests in the test suite
    // for Foo.
};

TEST(EngineEndToEndTest, DemoTest) {

    ASSERT_NO_FATAL_FAILURE({
#ifdef AE_OS_MACOS
        Atlas::Log::Warning(getenv("VK_LOADER_LAYERS_ENABLE"));
        Atlas::Log::Warning(getenv("DYLD_LIBRARY_PATH"));
        Atlas::Log::Warning(getenv("VK_ADD_LAYER_PATH"));
#endif
#ifdef AE_OS_WINDOWS
        int32_t sizeLoaderLayersEnable;
        int32_t sizeAddLayerPath;
        std::string loaderLayersEnable;
        std::string addLayerPath;
        loaderLayersEnable.resize(1024);
        addLayerPath.resize(1024);
        GetEnvironmentVariable("VK_LOADER_LAYERS_ENABLE", loaderLayersEnable.data(), sizeLoaderLayersEnable);
        GetEnvironmentVariable("VK_ADD_LAYER_PATH", addLayerPath.data(), sizeAddLayerPath);
        Atlas::Log::Warning(loaderLayersEnable);
        Atlas::Log::Warning(addLayerPath);
#endif
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
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
