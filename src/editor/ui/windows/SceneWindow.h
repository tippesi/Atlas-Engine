#pragma once

#include "Window.h"

#include "../panels/SceneHierarchyPanel.h"
#include "../panels/ViewportPanel.h"
#include "../panels/ScenePropertiesPanel.h"

#include "resource/Resource.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    class SceneWindow : public Window {

    public:
        explicit SceneWindow(ResourceHandle<Scene::Scene> scene, bool show);

        ~SceneWindow();

        bool IsValid() const { return scene.IsLoaded(); }

        void Update(float deltaTime);

        void Render();

        void RegisterViewportAndGizmoOverlay();

        void PushEntityIntoSceneHierarchy(Scene::Entity entity, bool changeSelection = true);

        void StartPlaying();

        void StopPlaying();

        void SaveSceneState();

        void RestoreSceneState();

        void SaveScene();

        SceneHierarchyPanel sceneHierarchyPanel;
        ViewportPanel viewportPanel;
        ScenePropertiesPanel scenePropertiesPanel;

        ResourceHandle<Scene::Scene> scene;

        Scene::Entity cameraEntity;

        bool snappingEnabled = false;
        float translationSnap = 0.1f;
        float rotationSnap = 1.0f;
        float scaleSnap = 0.01f;

        float cameraMovementSpeed = 7.0f;
        float cameraRotationSpeed = 1.5f;

        float resolutionScale = 0.75f;

        // Imguizmo translate mode
        int32_t guizmoMode = 7;
        bool needGuizmoEnabled = false;

        bool hasMainCamera = false;
        bool hasPlayer = false;
        bool isPlaying = false;
        bool isActiveWindow = false;
        bool lockSelection = false;

    private:
        void RenderEntityBoundingVolumes(Scene::Entity entity);

        std::vector<uint8_t> sceneState;
        std::vector<uint8_t> cameraState;

    };

}