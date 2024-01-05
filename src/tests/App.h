#pragma once

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>
#include <loader/ModelLoader.h>
#include <ImguiExtension/ImguiWrapper.h>

#include <renderer/PathTracingRenderer.h>
#include <renderer/ExampleRenderer.h>

#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI

struct AppConfiguration {
    bool clouds = true;
    bool sss = true;
    bool fog = true;
    bool taa = true;
    bool sharpen = false;
    bool ssgi = true;
    bool resize = false;
    bool recreateSwapchain = false;
    bool minimizeWindow = false;
    bool exampleRenderer = false;
};

class App : public Atlas::EngineInstance {

    template<class T>
    using Ref = Atlas::Ref<T>;

public:
    App() : EngineInstance("Atlas Engine Demo", 1920, 1080, WINDOW_FLAGS) {}

    virtual void LoadContent() final {};

    virtual void LoadContent(AppConfiguration config) final;

    virtual void UnloadContent() final;

    virtual void Update(float deltaTime) final;

    virtual void Render(float deltaTime) final;



private:
    void DisplayLoadingScreen(float deltaTime);

    bool LoadScene();
    void UnloadScene();
    void CheckLoadScene();

    void SetResolution(int32_t width, int32_t height);

    Atlas::Renderer::PathTracerRenderTarget pathTraceTarget;
    Atlas::RenderTarget renderTarget;
    Atlas::Viewport viewport;

    Atlas::Font font;

    Atlas::Camera camera;

    Ref<Atlas::Scene::Scene> scene;
    Ref<Atlas::Lighting::DirectionalLight> directionalLight;

    std::vector<Atlas::ResourceHandle<Atlas::Mesh::Mesh>> meshes;
    std::vector<Atlas::Actor::MovableMeshActor> actors;

    Atlas::Lighting::EnvironmentProbe probe;

    Atlas::Input::MouseHandler mouseHandler;
    Atlas::Input::KeyboardHandler keyboardHandler;

    Ref<Atlas::Texture::Texture2D> loadingTexture;

    Atlas::Renderer::ExampleRenderer exampleRenderer;

    bool renderUI = true;
    bool renderEnvProbe = true;
    bool spheresVisible = false;

    bool rotateCamera = false;
    bool moveCamera = false;
    float rotateCameraSpeed = 0.01f;
    float moveCameraSpeed = 0.1f;

    int32_t windowWidth = 1920;
    int32_t windowHeight = 1080;

    float cameraSpeed = 7.0f;

    bool loadingComplete = false;
    bool sceneReload = false;

    int32_t frameCount = 0;

    ImguiWrapper imguiWrapper;

    AppConfiguration config;

};