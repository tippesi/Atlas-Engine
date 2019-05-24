#include "Engine.h"
#include "EngineInstance.h"

extern Atlas::EngineInstance* GetEngineInstance();

int main(int argc, char* argv[]) {

	auto context = Atlas::Engine::Init(Atlas::EngineInstance::assetDirectory,
		Atlas::EngineInstance::shaderDirectory);

	auto instance = GetEngineInstance();

	// No need to clean the context up, will be released
	// when the instance is being deleted.
	instance->context = *context;

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

	return 0;

}