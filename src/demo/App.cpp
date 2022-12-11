#include "App.h"

const std::string Atlas::EngineInstance::assetDirectory = "../data";
const std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

    testRenderer.Init(graphicInstance->GetGraphicsDevice());

}

void App::UnloadContent() {



}

void App::Update(float deltaTime) {



}

void App::Render(float deltaTime) {

    testRenderer.Render();

}

Atlas::EngineInstance* GetEngineInstance() {

    return new App();

}