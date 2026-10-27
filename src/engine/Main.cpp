#include "Engine.h"
#include "EngineInstance.h"
#include "graphics/Instance.h"
#include "common/Path.h"

#if defined(AE_OS_ANDROID) || defined(AE_OS_MACOS) || defined(AE_OS_APPLE_MOBILE) || defined(AE_OS_LINUX)
#include <zconf.h>
#endif

#ifdef AE_OS_WINDOWS
#include <direct.h>
#include <Windows.h>
#endif

extern Atlas::EngineInstance* GetEngineInstance();

int main(int argc, char* argv[]) {
    
    // Automatically change working directory to load
    // shaders properly.
#if !defined(AE_OS_APPLE_MOBILE)
    if (argc > 0) {
        auto workingDir = Atlas::Common::Path::GetDirectory(argv[0]);
#ifdef AE_OS_WINDOWS
        _chdir(workingDir.c_str());
#else
        chdir(workingDir.c_str());
#endif
    }
#endif

    // To test with swiftshader locally, put in the path of the *_icd.json (note: on Windows use backslashes)
    // SetEnvironmentVariable("VK_ICD_FILENAMES", "..\\vk_swiftshader_icd.json");

    // Validation has to be known before the graphics instance is created
    auto engineConfig = Atlas::EngineInstance::engineConfig;
    for (int32_t i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--validation") {
            engineConfig.deviceDesc.enableValidationLayers = true;
            engineConfig.deviceDesc.enableExtendedValidation = true;
        }
    }

    Atlas::Engine::Init(engineConfig);

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

    engineInstance->LoadContent();

    // Update now such the first delta is actually valid and not skewed by the loading
    Atlas::Clock::Update();

    while (!quit) {

        Atlas::Engine::Update();
        
        auto deltaTime = Atlas::Clock::GetDelta();

        engineInstance->Update();

        engineInstance->Update(deltaTime);
        engineInstance->Render(deltaTime);

        graphicsDevice->SubmitFrame();

        if (graphicsDevice->isDeviceLost)
            break;

    }

    engineInstance->UnloadContent();
    auto exitCode = graphicsDevice->isDeviceLost ? -1 : engineInstance->GetExitCode();
    delete engineInstance;

    Atlas::Engine::Shutdown();
    delete graphicsInstance;

    return exitCode;

}
