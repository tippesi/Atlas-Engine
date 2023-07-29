#ifndef AE_APP_H
#define AE_APP_H

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>
#include <loader/ModelLoader.h>
#include <ImguiExtension/ImguiWrapper.h>

#include <renderer/PathTracingRenderer.h>

#define WINDOW_FLAGS AE_WINDOW_RESIZABLE | AE_WINDOW_HIGH_DPI

class App : public Atlas::EngineInstance {

    template<class T>
    using Ref = Atlas::Ref<T>;

public:
    App() : EngineInstance("Atlas Engine Demo", 1920, 1080, WINDOW_FLAGS) {}

    virtual void LoadContent() final;

    virtual void UnloadContent() final;

    virtual void Update(float deltaTime) final;

    virtual void Render(float deltaTime) final;

private:
    enum SceneSelection {
        CORNELL = 0,
        SPONZA,
        SANMIGUEL,
        NEWSPONZA,
        BISTRO,
        MEDIEVAL,
        PICAPICA,
        SUBWAY,
        MATERIALS,
        FOREST
    };

    void DisplayLoadingScreen(float deltaTime);

    bool IsSceneAvailable(SceneSelection selection);
    bool LoadScene();
    void UnloadScene();
    void CheckLoadScene();

    void SetResolution(int32_t width, int32_t height);

    void CopyActors(Ref<Atlas::Scene::Scene> otherScene);

    SceneSelection sceneSelection = SPONZA;

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

    ImguiWrapper imguiWrapper;

};

#endif