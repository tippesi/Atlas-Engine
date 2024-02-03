#pragma once

#include "Panel.h"
#include "scene/Scene.h"
#include "Viewport.h"

namespace Atlas::Editor::UI {

    class ViewportPanel : public Panel {

    public:
        ViewportPanel();

        void DrawOverlay(std::function<void()> func);

        void Render(Ref<Scene::Scene>& scene, bool isParentFocused);

        Ref<Viewport> viewport;
        Texture::Texture2D viewportTexture;

        std::function<void()> drawOverlayFunc;

    };

}
