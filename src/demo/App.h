#ifndef AE_APP_H
#define AE_APP_H

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>

#include <renderer/VulkanTestRenderer.h>
#include <ImguiExtension/ImguiWrapper.h>

#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI

class App : public Atlas::EngineInstance {

public:
    App() : EngineInstance("Atlas Engine", 1280, 720, WINDOW_FLAGS) {}

    virtual void LoadContent() final;

    virtual void UnloadContent() final;

    virtual void Update(float deltaTime) final;

    virtual void Render(float deltaTime) final;

private:
    Atlas::Renderer::VulkanTestRenderer testRenderer;

    Atlas::Camera camera;
    Atlas::Input::MouseHandler mouseHandler;
    Atlas::Input::KeyboardHandler keyboardHandler;

    ImguiWrapper imguiWrapper;


};

#endif