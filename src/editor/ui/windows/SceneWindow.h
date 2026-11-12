#pragma once

#include "Window.h"

#include "../../tools/Guizmo.h"
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

        void TogglePause();

        void StopPlaying();

        void SaveSceneState();

        void RestoreSceneState();

        void SaveScene();

        // Loads a prefab into the scene of the window, saving writes the prefab instead of the scene
        void OpenPrefab(const std::string& path);

        bool IsPrefab() const { return !prefabPath.empty(); }

        // The file of the scene, which it is saved to
        static std::string GetScenePath(const ResourceHandle<Scene::Scene>& scene);

        // Name of the file without its extension, the tab shows it
        static std::string GetSceneLabel(const ResourceHandle<Scene::Scene>& scene);

        // Moves the editor camera such that the selected entities are in view
        void FocusSelection();

        // Moves the selected entities down until their bounds rest on the surface below them
        void DropSelectionToGround();

        // Creates a predefined entity in front of the editor camera and selects it.
        // An invalid parent means the root entity
        Scene::Entity CreateEntityFromTemplate(SceneHierarchyPanel::EntityTemplate type, Scene::Entity parent = Scene::Entity());

        // Moves the editor camera to a scene camera, the view can be changed and applied to it
        void ViewThroughCamera(Scene::Entity entity);

        // Writes the editor camera view into the scene camera it was moved to
        void ApplyViewToCamera();

        void ReturnToEditorCamera();

        void ApplyBrush();

        // Draws the navigation mesh and the test path, right clicks set the points of the path
        void ApplyNavigationTools();

        // Applied right before the next scene update, the panels would otherwise read
        // the restored transforms with outdated global matrices
        void Undo();

        void Redo();

        SceneHierarchyPanel sceneHierarchyPanel;
        ViewportPanel viewportPanel;
        ScenePropertiesPanel scenePropertiesPanel;

        ResourceHandle<Scene::Scene> scene;

        std::string prefabPath;

        Scene::Entity cameraEntity;
        Scene::Entity viewedCameraEntity;

        Guizmo guizmo;

        float cameraMovementSpeed = 7.0f;
        float cameraRotationSpeed = 1.5f;

        float resolutionScale = 0.75f;       

        bool depthTestBoundingVolumes = false;
        bool showGrid = false;
        bool showPhysics = false;
        bool depthTestPhysics = true;

        Physics::DebugDrawSettings physicsDebugDrawSettings;

        // Shown as a red star in the title, cleared when saving
        bool hasUnsavedChanges = false;

        SceneHistory history;

        bool hasMainCamera = false;
        bool hasPlayer = false;
        bool isPlaying = false;
        bool playMaximized = false;
        bool perfOverlayMaximized = false;
        bool isActiveWindow = false;
        bool lockSelection = false;
        bool brushActive = false;

    private:
        void RenderEntityBoundingVolumes(Scene::Entity entity);

        void ApplyRequestedHistorySteps();

        // Selects what the last undo or redo changed
        void ApplyHistoryChange();

        // Physics bodies, contacts and constraints around the main camera
        void RenderPhysics();

        int32_t requestedHistorySteps = 0;

        // Time when the camera speed was last changed with the mouse wheel, to show a short hint
        float cameraSpeedChangeTime = -1000.0f;

        void PerformEntitySelection();

        // Whether the entity or one of its ancestors is selected, such that rays ignore what they place
        bool IsInSelection(Scene::Entity entity);

        std::vector<uint8_t> sceneState;
        std::vector<uint8_t> cameraState;
        // Editor camera from before a scene camera was viewed
        CameraComponent editorCameraBackup;

        // Stays valid when the scene is restored after playing
        ECS::Entity prefabRoot = ECS::EntityConfig::InvalidEntity;
        // Adding the prefab to the scene hierarchy clears the flag, the file keeps it
        bool prefabRootIsHierarchyRoot = false;
        bool focusAfterUpdate = false;

        float terrainFlattenHeight = Terrain::Terrain::invalidHeight;
        bool terrainHeightsEdited = false;

    };

}