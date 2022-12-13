#include "App.h"

const std::string Atlas::EngineInstance::assetDirectory = "../data";
const std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

    testRenderer.Init(graphicInstance->GetGraphicsDevice());

    camera = Atlas::Camera(47.0f, 2.0f, 1.0f, 400.0f,
        glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));

    mouseHandler = Atlas::Input::MouseHandler(&camera, 1.5f, 6.0f);
    keyboardHandler = Atlas::Input::KeyboardHandler(&camera, 7.0f, 6.0f);

}

void App::UnloadContent() {



}

void App::Update(float deltaTime) {

    mouseHandler.Update(&camera, deltaTime);
    keyboardHandler.Update(&camera, deltaTime);

    camera.Update();

}

void App::Render(float deltaTime) {

    testRenderer.Render(&camera);

}

Atlas::EngineInstance* GetEngineInstance() {

    return new App();

}