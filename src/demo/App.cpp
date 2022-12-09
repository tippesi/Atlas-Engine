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

    auto blue = abs(sin(Atlas::Clock::Get()));

    auto device = graphicInstance->GetGraphicsDevice();
    auto commandList = device->GetCommandList(Atlas::Graphics::GraphicsQueue);
    auto swapChain = device->swapChain;

    commandList->BeginCommands();

    swapChain->clearValue.color = { 0.0f, 0.0f, blue, 1.0f};
    commandList->BeginRenderPass(swapChain);


    commandList->EndRenderPass();
    commandList->EndCommands();

    device->SubmitCommandList(commandList);
    device->CompleteFrame();

}

Atlas::EngineInstance* GetEngineInstance() {

    return new App();

}