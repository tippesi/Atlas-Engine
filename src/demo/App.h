#pragma once

#include <EngineInstance.h>
#include <input/Mouse.h>
#include <input/Keyboard.h>
#include <input/Controller.h>
#include <input/Touch.h>
#include <loader/ModelLoader.h>

#include <ImguiExtension/ImguiWrapper.h>
#include <ImguiExtension/Panels.h>

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
        FOREST,
        EMERALDSQUARE,
        FLYINGWORLD
    };

    void DisplayLoadingScreen(float deltaTime);

    bool IsSceneAvailable(SceneSelection selection);
    bool LoadScene();
    void UnloadScene();
    void CheckLoadScene();

    void SetResolution(int32_t width, int32_t height);

    void CopyActors(Ref<Atlas::Scene::Scene> otherScene);

    SceneSelection sceneSelection = SPONZA;

    Ref<Atlas::Renderer::PathTracerRenderTarget> pathTraceTarget;
    Ref<Atlas::Renderer::RenderTarget> renderTarget;
    Ref<Atlas::Viewport> viewport;

    Ref<Atlas::Font> font;

    Ref<Atlas::Scene::Scene> scene;

    std::vector<Atlas::ResourceHandle<Atlas::Mesh::Mesh>> meshes;

    Atlas::Scene::Entity cameraEntity;
    Atlas::Scene::Entity directionalLightEntity;
    std::vector<Atlas::Scene::Entity> entities;

    Atlas::Lighting::EnvironmentProbe probe;

    Atlas::Input::MouseHandler mouseHandler;
    Atlas::Input::KeyboardHandler keyboardHandler;
    Atlas::Input::ControllerHandler controllerHandler;

    Ref<Atlas::Texture::Texture2D> loadingTexture;

    Atlas::ImguiExtension::FogPanel fogPanel;
    Atlas::ImguiExtension::VolumetricCloudsPanel volumetricCloudsPanel;
    Atlas::ImguiExtension::IrradianceVolumePanel irradianceVolumePanel;
    Atlas::ImguiExtension::ReflectionPanel reflectionPanel;
    Atlas::ImguiExtension::SSGIPanel ssgiPanel;
    Atlas::ImguiExtension::SSSPanel sssPanel;
    Atlas::ImguiExtension::PostProcessingPanel postProcessingPanel;
    Atlas::ImguiExtension::GPUProfilerPanel gpuProfilerPanel;
    Atlas::ImguiExtension::MaterialsPanel materialsPanel;
    Atlas::ImguiExtension::WindPanel windPanel;

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

    float sphereScale = 1.0f;
    float sphereDensity = 1.0f;
    float sphereRestitution = 0.2f;

    bool emitSpheresEnabled = false;
    float emitSpawnRate = 0.1f;

    bool shootSpheresEnabled = false;
    bool shootSphere = false;
    float shootSpawnRate = 0.1f;
    float shootVelocity = 5.0f;

    Ref<Atlas::ImguiExtension::ImguiWrapper> imguiWrapper;

    std::vector<Ref<Atlas::Audio::AudioStream>> audioStreams;

};