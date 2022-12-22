#include "App.h"

const std::string Atlas::EngineInstance::assetDirectory = "../data";
const std::string Atlas::EngineInstance::shaderDirectory = "shader";

void App::LoadContent() {

    testRenderer.Init(Atlas::Graphics::GraphicsDevice::DefaultDevice);

    camera = Atlas::Camera(47.0f, 2.0f, 1.0f, 400.0f,
        glm::vec3(30.0f, 25.0f, 0.0f), glm::vec2(-3.14f / 2.0f, 0.0f));

    mouseHandler = Atlas::Input::MouseHandler(&camera, 1.5f, 6.0f);
    keyboardHandler = Atlas::Input::KeyboardHandler(&camera, 7.0f, 6.0f);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    imguiWrapper.Load(window);

    /*
    io.Fonts->AddFontFromFileTTF(
        Atlas::Loader::AssetLoader::GetFullPath("font/roboto.ttf").c_str(),
        20.0f);
    */

    ImGui::StyleColorsDark();
    for (auto& color : ImGui::GetStyle().Colors) {
        float gamma = 2.2f;
        color.x = powf(color.x, gamma);
        color.y = powf(color.y, gamma);
        color.z = powf(color.z, gamma);
    }

}

void App::UnloadContent() {

    imguiWrapper.Unload();

}

void App::Update(float deltaTime) {

    mouseHandler.Update(&camera, deltaTime);
    keyboardHandler.Update(&camera, deltaTime);

    camera.Update();
    imguiWrapper.Update(window, deltaTime);

}

void App::Render(float deltaTime) {

    testRenderer.Render(&camera);

    ImGui::NewFrame();

    ImGui::ShowDemoWindow();

    ImGui::Render();
    imguiWrapper.Render();

}

Atlas::EngineInstance* GetEngineInstance() {

    return new App();

}