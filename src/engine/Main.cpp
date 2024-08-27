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

extern Atlas::EngineInstance* GetEngineInstance();

int main(int argc, char* argv[]) {

    printf("Start");

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

    printf("Args");

#if defined(AE_OS_MACOS) && defined(AE_BINDLESS)
    setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "2", 1);
    setenv("MVK_DEBUG", "0", 1);
#elif defined(AE_OS_MACOS) && defined(AE_BINDLESS)
    setenv("MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS", "0", 1);
#endif

    printf("Envs");
    
    // To test with swiftshader locally, put in the path of the *_icd.json (note: on Windows use backslashes)
    // SetEnvironmentVariable("VK_ICD_FILENAMES", "..\\vk_swiftshader_icd.json");

    Atlas::Engine::Init(Atlas::EngineInstance::engineConfig);

    printf("Init");

    auto graphicsInstance = Atlas::Graphics::Instance::DefaultInstance;

    if (!graphicsInstance->isComplete) {
        Atlas::Log::Error("Couldn't initialize graphics instance");
        Atlas::Engine::Shutdown();
        return 0;
    }

    // Only then create engine instance. This makes sure that the engine instance already
    // has access to all graphics functionality and all other functionality on construction
    auto engineInstance = GetEngineInstance();
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

    printf("Ready");

    engineInstance->LoadContent();

    printf("Load content");

    // Update now such the first delta is actually valid and not skewed by the loading
    Atlas::Clock::Update();

    while (!quit) {

        Atlas::Engine::Update();
        
        auto deltaTime = Atlas::Clock::GetDelta();

        engineInstance->Update();

        engineInstance->Update(deltaTime);
        engineInstance->Render(deltaTime);

        graphicsDevice->SubmitFrame();
        
    }

    engineInstance->UnloadContent();
    delete engineInstance;

    Atlas::Engine::Shutdown();
    delete graphicsInstance;

    return 0;

}
