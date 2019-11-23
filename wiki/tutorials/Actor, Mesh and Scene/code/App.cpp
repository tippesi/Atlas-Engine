#include "App.h"

std::string Atlas::EngineInstance::assetDirectory = "../data";
std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

    viewport = Atlas::Viewport(0, 0, window.GetWidth(), window.GetHeight());

    renderTarget = new Atlas::RenderTarget(1920, 1080);



}

void App::UnloadContent() {



}

void App::Update(float deltaTime) {



}

void App::Render(float deltaTime) {

    viewport.Set(0, 0, window.GetWidth(), window.GetHeight());



}

Atlas::EngineInstance* GetEngineInstance() {

	return new App();

}