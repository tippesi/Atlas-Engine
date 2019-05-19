#include "Engine.h"
#include "EngineInstance.h"

extern Atlas::EngineInstance* GetEngineInstance();

int main(int argc, char* argv[]) {

	auto instance = GetEngineInstance();

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

		engine->window->Update();

	}

	instance->UnloadContent();

	return 0;

}