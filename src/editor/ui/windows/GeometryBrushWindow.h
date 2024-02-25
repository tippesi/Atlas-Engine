#pragma once

#include "Window.h"
#include "SceneWindow.h"

#include "scene/Scene.h"
#include "Viewport.h"

namespace Atlas::Editor::UI {

    class GeometryBrushWindow : public Window {

    public:
        explicit GeometryBrushWindow(bool show) : Window("Geometry brush", show) {}

        void Render(const Ref<SceneWindow>& activeSceneWindow);

        bool brushEnabled = false;

    private:
        struct DropTarget {
            vec3 center;
            vec3 normal;
            vec3 tangent;
            vec3 bitangent;

            bool valid = false;
        };

        Scene::Entity brushEntity;
        Scene::Entity parentEntity;

        float brushRadius = 5.0f;
        float brushRayLength = 5.0f;
        bool brushAlignToSurface = false;
        int32_t brushDropRate = 10.0f;

        float brushLastApplied = 0.0f;

        void RenderBrushSettings();

        DropTarget CalculateDropTarget(const Ref<SceneWindow>& activeSceneWindow);

        void ApplyBrush(const Ref<SceneWindow>& activeSceneWindow, const DropTarget& dropTarget);

    };

}