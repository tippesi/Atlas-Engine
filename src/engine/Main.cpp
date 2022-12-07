#include "Engine.h"
#include "EngineInstance.h"
#include "common/Path.h"

#if defined(AE_OS_ANDROID) || defined(AE_OS_MACOS) || defined(AE_OS_LINUX)
#include <zconf.h>
#endif

#ifdef AE_OS_WINDOWS
#include <direct.h>
#endif

extern Atlas::EngineInstance* GetEngineInstance();

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

	auto context = Atlas::Engine::Init(Atlas::EngineInstance::assetDirectory,
		Atlas::EngineInstance::shaderDirectory);

	if (!context) {
		Atlas::Log::Warning("Shutdown of application");
		Atlas::Engine::Shutdown();
		return 0;
	}

	auto instance = GetEngineInstance();

	// No need to clean the context up, will be released
	// when the instance is being deleted.
	// instance->context = *context;

	// We need to pass the command line arguments
	for (int32_t i = 0; i < argc; i++)
	    instance->args.push_back(std::string(argv[i]));

	bool quit = false;

	Atlas::Events::EventManager::QuitEventDelegate.Subscribe(
		[&quit]() {
			quit = true;
	});

	instance->LoadContent();

	while (!quit) {

		Atlas::Engine::Update();
		
		auto deltaTime = Atlas::Clock::GetDelta();

		instance->Update();

		instance->Update(deltaTime);
		instance->Render(deltaTime);

		instance->window.Update();

	}

	instance->UnloadContent();

	Atlas::Engine::Shutdown();

    // Need this right now just for linking to be successful
    if (!gladLoadGL()) {

    }

	return 0;

}