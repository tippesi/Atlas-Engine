#include "App.h"

std::string Atlas::EngineInstance::assetDirectory = "../data";
std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

    viewport = Atlas::Viewport(0, 0, window.GetWidth(), window.GetHeight());

    font = Atlas::Font("roboto.ttf", 88, 5);

}

void App::UnloadContent() {



}

void App::Update(float deltaTime) {



}

void App::Render(float deltaTime) {

    viewport.Set(0, 0, window.GetWidth(), window.GetHeight());

    std::string text = "Hello World!";

    float width, height;
    font.ComputeDimensions(text, 1.0f, &width, &height);

    auto x = (float)window.GetWidth() / 2.0f - width / 2.0f;
    auto y = (float)window.GetHeight() / 2.0f - height / 2.0f;

    masterRenderer.textRenderer.Render(&viewport, &font, text,
            x, y);

}

Atlas::EngineInstance* GetEngineInstance() {

	return new App();

}