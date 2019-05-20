#include "Engine.h"
#include "EngineInstance.h"

extern Atlas::EngineInstance* GetEngineInstance();

int main(int argc, char* argv[]) {

	auto context = Atlas::Engine::Init(Atlas::EngineInstance::assetDirectory,
		Atlas::EngineInstance::shaderDirectory);

	auto instance = GetEngineInstance();

	context->AttachTo(&instance->window);

	// No need to clean context up, will be released when the
	// instance is deleted.
	instance->context = *context;

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

	return 0;

}