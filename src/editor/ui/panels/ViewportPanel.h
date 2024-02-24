#pragma once

#include "Panel.h"
#include "tools/PrimitiveBatchWrapper.h"
#include "scene/Scene.h"
#include "Viewport.h"

namespace Atlas::Editor::UI {

    class ViewportPanel : public Panel {

    public:
        ViewportPanel();

        void DrawMenuBar(std::function<void()> func);

        void DrawOverlay(std::function<void()> func);

        void Render(Ref<Scene::Scene>& scene, bool isActiveWindow);

        Ref<Viewport> viewport;
        Texture::Texture2D viewportTexture;

        PrimitiveBatchWrapper primitiveBatchWrapper;

        std::function<void()> drawMenuBarFunc;
        std::function<void()> drawOverlayFunc;

    private:
        bool firstFrame = true;

    };

}
