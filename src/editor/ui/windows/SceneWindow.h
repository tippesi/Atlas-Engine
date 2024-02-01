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
        explicit SceneWindow(ResourceHandle<Scene::Scene> scene) :
            Window(scene.IsLoaded() ? scene->name : "No scene"), scene(scene) {}

        ~SceneWindow();

        bool IsValid() const { return scene.IsLoaded(); }

        void Update(float deltaTime);

        void Render();

        SceneHierarchyPanel sceneHierarchyPanel;
        ViewportPanel viewportPanel;
        ScenePropertiesPanel entityPropertiesPanel;

        ResourceHandle<Scene::Scene> scene;

        Scene::Entity cameraEntity;

    };

}