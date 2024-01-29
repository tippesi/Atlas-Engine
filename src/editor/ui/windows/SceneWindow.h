#pragma once

#include "Window.h"

#include "resource/Resource.h"
#include "scene/Scene.h"

namespace Atlas::Editor::UI {

    class SceneWindow : public Window {

    public:
        explicit SceneWindow(ResourceHandle<Scene::Scene> scene) :
            Window(scene.IsLoaded() ? scene->name : "No scene"), scene(scene) {}

        bool IsValid() const { return scene.IsLoaded(); }

        void Render();

        ResourceHandle<Scene::Scene> scene;

    };

}