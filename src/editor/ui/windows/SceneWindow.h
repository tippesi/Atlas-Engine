#pragma once

#include "Window.h"

#include "../../tools/AssetPlacer.h"
#include "../../tools/Guizmo.h"
#include "../SceneOverlays.h"
#include "../panels/SceneHierarchyPanel.h"
#include "../panels/ViewportPanel.h"
#include "../panels/ScenePropertiesPanel.h"

#include "resource/Resource.h"
#include "scene/Scene.h"
#include "scripting/LuaHost.h"

#include <map>
#include <memory>
#include <optional>

namespace Atlas::Editor::UI {

    struct ViewDirection {
        const char* label;
        const char* name;
        vec2 rotation;
    };

    struct CameraBookmark {
        vec3 location = vec3(0.0f);
        vec2 rotation = vec2(0.0f);
    };

    // The camera from before it became orthographic
    struct OrthographicView {
        bool active = false;
        float focusDistance = 20.0f;
        float fieldOfView = 47.0f;
        float nearPlane = 1.0f;
        float farPlane = 2000.0f;
    };

    class SceneWindow : public Window {

    public:
        explicit SceneWindow(ResourceHandle<Scene::Scene> scene, bool show);

        ~SceneWindow();

        bool IsValid() const { return scene.IsLoaded(); }

        void Update(float deltaTime);

        void Render();

        void RegisterViewportAndGizmoOverlay();

        void PushEntityIntoSceneHierarchy(Scene::Entity entity, bool changeSelection = true);

        // Starts where the editor camera is: a player is placed below it, without one the camera flies as while editing.
        // Scenes without a main camera always start this way. Returns whether the scene starts to play
        bool StartPlaying(bool fromView = false);

        // Runs the paused scene for one step
        void StepFrame();

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

        // Looks at the selection, or the surface in the center of the view, along an axis
        void SetViewRotation(vec2 rotation);

        // Looks from far away with a small field of view, such that parallel lines nearly stay parallel. A real
        // orthographic projection isn't supported by the renderers, e.g. their rays start at the camera
        void SetOrthographic(bool orthographic);

        // Where a camera with the usual field of view would be, distances are measured from there
        vec3 GetViewLocation();

        void SetBookmark(int32_t index);

        void GoToBookmark(int32_t index);

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
        // The flying camera while playing without a player
        Scene::Entity playCameraEntity;

        static const ViewDirection viewDirections[6];

        std::map<int32_t, CameraBookmark> bookmarks;
        static constexpr int32_t maxBookmarks = 9;

        OrthographicView orthographicView;

        Guizmo guizmo;

        float cameraMovementSpeed = 7.0f;
        float cameraRotationSpeed = 1.5f;

        float resolutionScale = 0.75f;       

        bool showGrid = false;
        bool showPhysics = false;
        bool depthTestPhysics = true;

        SceneOverlays overlays;

        Physics::DebugDrawSettings physicsDebugDrawSettings;

        SceneHistory history;

        bool isPlaying = false;
        bool playMaximized = false;
        bool perfOverlayMaximized = false;
        bool isActiveWindow = false;
        bool lockSelection = false;
        bool brushActive = false;
        // Entities which were just created have their place and bounds after the scene was updated
        bool focusAfterUpdate = false;

    private:
        // Assets dragged from the content browser are dropped on what is under the mouse
        void AcceptAssetDrop();

        void PlaceDroppedAssets();

        // Toolbar above the viewport with the camera popups and the camera speed hint
        void RenderViewportToolbar();

        // Left: gizmo operations
        void RenderGizmoToolbar(float margin);

        // Center: play controls
        void RenderPlayControls(float width);

        // Right: viewport settings and visualization
        void RenderViewportSettingsToolbar(float margin, float width);

        // Camera speed, gizmos and the selection with right clicks in the viewport
        void RenderViewportOverlay();

        void ApplyRequestedHistorySteps();

        // Selects what the last undo or redo changed
        void ApplyHistoryChange();

        // Physics bodies, contacts and constraints around the main camera
        void RenderPhysics();

        void NotifySaved(bool saved, const std::string& label);

        // The first player with input, or the first one
        Scene::Entity FindPlayer();

        // The player or the flying camera of a scene that plays from the view
        void PlaceInView(const CameraComponent& view, bool hasMainCamera);

        // Bounds of the selected entities, small ones around entities without meshes
        std::optional<Volume::AABB> GetSelectionBounds();

        // The center of the selection, or the surface the camera looks at
        vec3 GetFocusPoint();

        float GetOrthographicDistance() const;

        // Where the editor camera looks, it only updates itself with the scene
        static vec3 DirectionFromRotation(vec2 rotation);

        // Moves the orthographic camera to its distance from a point
        void UpdateOrthographicCamera(vec3 focusPoint);

        AssetPlacer assetPlacer;

        bool stepRequested = false;
        static constexpr float stepTime = 1.0f / 60.0f;
        static constexpr float orthographicFieldOfView = 4.0f;

        int32_t requestedHistorySteps = 0;

        // Time when the camera speed was last changed with the mouse wheel, to show a short hint
        float cameraSpeedChangeTime = -1000.0f;

        void PerformEntitySelection();

        // Whether the entity or one of its ancestors is selected, such that rays ignore what they place
        bool IsInSelection(Scene::Entity entity);

        static constexpr size_t maxSelectedHistoryEntities = 1000;

        std::vector<uint8_t> sceneState;
        std::vector<uint8_t> cameraState;
        // Main script of the project while playing, a new one each time playing starts
        std::unique_ptr<Scripting::LuaHost> mainScript;
        // Editor camera from before a scene camera was viewed
        CameraComponent editorCameraBackup;

        // Stays valid when the scene is restored after playing
        ECS::Entity prefabRoot = ECS::EntityConfig::InvalidEntity;
        // Adding the prefab to the scene hierarchy clears the flag, the file keeps it
        bool prefabRootIsHierarchyRoot = false;

        float terrainFlattenHeight = Terrain::Terrain::invalidHeight;
        bool terrainHeightsEdited = false;

    };

}