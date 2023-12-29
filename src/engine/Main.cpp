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

int main(int argc, char* argv[]) {

    // Automatically change working directory to load
    // shaders properly.
    if (argc > 0) {
        auto workingDir = Atlas::Common::Path::GetDirectory(argv[0]);
#ifdef AE_OS_WINDOWS
        _chdir(workingDir.c_str());
#else
        chdir(workingDir.c_str());
#endif
    }

#ifdef AE_OS_MACOS
    setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "2", 1);
    setenv("MVK_DEBUG", "0", 1);
#endif

#ifdef AE_OS_WINDOWS
    SetEnvironmentVariable("VK_ICD_FILENAMES", "D:\\a\\Atlas-Engine\\Atlas-Engine\\swiftshader\\build\\Windows\\vk_swiftshader_icd.json");
#endif

    Atlas::Engine::Init(Atlas::EngineInstance::engineConfig);

    auto graphicsInstance = Atlas::Graphics::Instance::DefaultInstance;

    if (!graphicsInstance->isComplete) {
        Atlas::Log::Error("Couldn't initialize graphics instance");
        Atlas::Engine::Shutdown();
        return 0;
    }

    auto engineInstance = Atlas::EngineInstance::GetInstance();
    if (!engineInstance) {
        Atlas::Log::Warning("Shutdown of application");
        Atlas::Engine::Shutdown();
        return 0;
    }

    auto graphicsDevice = graphicsInstance->GetGraphicsDevice();

    // We need to pass the command line arguments
    for (int32_t i = 0; i < argc; i++)
        engineInstance->args.push_back(std::string(argv[i]));

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

    return 0;

}
