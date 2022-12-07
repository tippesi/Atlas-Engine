#include "App.h"

const std::string Atlas::EngineInstance::assetDirectory = "../data";
const std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {



}

void App::UnloadContent() {



}

void App::Update(float deltaTime) {



}

void App::Render(float deltaTime) {



}

Atlas::EngineInstance* GetEngineInstance() {

    return new App();

}