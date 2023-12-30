#include <gtest/gtest.h>
#include "Engine.h"
#include "EngineInstance.h"
#include "graphics/Instance.h"
#include "common/Path.h"
#include "App.h"

#if defined(AE_OS_ANDROID) || defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
#include <zconf.h>
#endif

#ifdef AE_OS_WINDOWS
#include <direct.h>
#include <Windows.h>
#endif

extern Atlas::EngineInstance* GetEngineInstance();

// The fixture for testing class Foo.
class EngineEndToEndTest : public testing::TestWithParam<AppConfiguration> {
protected:
    void SetUp() override {
        graphicsInstance = Atlas::Graphics::Instance::DefaultInstance;
        graphicsDevice = Atlas::Graphics::GraphicsDevice::DefaultDevice;

        engineInstance = GetEngineInstance();
        ASSERT_NE(engineInstance, nullptr);
    }

    void TearDown() override {
        delete engineInstance;

        Atlas::PipelineManager::Clear();

        graphicsDevice->ForceMemoryCleanup();
    }

    Atlas::Graphics::Instance* graphicsInstance = nullptr;
    Atlas::Graphics::GraphicsDevice* graphicsDevice = nullptr;
    Atlas::EngineInstance* engineInstance = nullptr;

public:
    static void SetUpTestSuite()  {
        Atlas::Engine::Init(Atlas::EngineInstance::engineConfig);

        auto graphicsInstance = Atlas::Graphics::Instance::DefaultInstance;
        ASSERT_EQ(graphicsInstance->isComplete, true);
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

            graphicsDevice->CompleteFrame();

        }

        engineInstance->UnloadContent();

        });
}

auto testingValues = testing::Values(
    AppConfiguration { .sss = false },
    AppConfiguration { .clouds = false },
    AppConfiguration { .fog = false },
    AppConfiguration { .taa = false },
    AppConfiguration { .sharpen = false }
    );

INSTANTIATE_TEST_SUITE_P(DemoTestSuite, EngineEndToEndTest, testingValues);

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);

    return RUN_ALL_TESTS();
}
