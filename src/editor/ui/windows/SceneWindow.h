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
        explicit SceneWindow(ResourceHandle<Scene::Scene> scene);

        ~SceneWindow();

        bool IsValid() const { return scene.IsLoaded(); }

        void Update(float deltaTime);

        void Render();

        void RegisterViewportAndGizmoOverlay();

        SceneHierarchyPanel sceneHierarchyPanel;
        ViewportPanel viewportPanel;
        ScenePropertiesPanel scenePropertiesPanel;

        ResourceHandle<Scene::Scene> scene;

        Scene::Entity cameraEntity;

        // Imguizmo translate mode
        int32_t guizmoMode = 7;
        bool needGuizmoEnabled = false;

        bool hasMainCamera = false;
        bool hasPlayer = false;
        bool isPlaying = false;

    private:
        void RenderEntityBoundingVolumes(Scene::Entity entity);

    };

}