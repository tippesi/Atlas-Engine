#include "Engine.h"
#include "EngineInstance.h"
#include "graphics/Instance.h"
#include "common/Path.h"
#include "profiler/CPUProfiler.h"

#include <SDL3/SDL_main.h>

#if defined(AE_OS_ANDROID) || defined(AE_OS_MACOS) || defined(AE_OS_APPLE_MOBILE) || defined(AE_OS_LINUX)
#include <zconf.h>
#endif

#ifdef AE_OS_WINDOWS
#include <direct.h>
#include <Windows.h>
#endif

extern Atlas::EngineInstance* GetEngineInstance();

int main(int argc, char* argv[]) {

    Atlas::CPUProfiler::SetThreadName("Main");
    
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

    auto engineConfig = Atlas::EngineInstance::engineConfig;
    if (engineConfig.commandLineTool) {
        if (auto exitCode = engineConfig.commandLineTool(std::vector<std::string>(argv, argv + argc)))
            return *exitCode;
    }

    // Validation has to be known before the graphics instance is created, shaders can come from another directory
    for (int32_t i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--validation") {
            engineConfig.deviceDesc.enableValidationLayers = true;
            engineConfig.deviceDesc.enableExtendedValidation = true;
        }
        if (std::string(argv[i]) == "--shader-dir" && i + 1 < argc)
            engineConfig.shaderDirectory = argv[i + 1];
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

        Atlas::Engine::Update(engineInstance->UsesRenderThread());
        
        auto deltaTime = Atlas::Clock::GetDelta();

        engineInstance->Update();

        {
            Atlas::CPUProfiler::Scope profilerScope("Update");
            engineInstance->Update(deltaTime);
        }
        if (!engineInstance->RenderFrame(deltaTime))
            break;

    }

    engineInstance->WaitForRender();
    engineInstance->UnloadContent();
    auto exitCode = graphicsDevice->isDeviceLost ? -1 : engineInstance->GetExitCode();
    delete engineInstance;

    Atlas::Engine::Shutdown();
    delete graphicsInstance;

    return exitCode;

}
