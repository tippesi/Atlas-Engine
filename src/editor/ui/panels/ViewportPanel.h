#pragma once

#include "Panel.h"
#include "tools/PrimitiveBatchWrapper.h"
#include "scene/Scene.h"
#include "Viewport.h"

namespace Atlas::Editor::UI {

    enum ViewportVisualization {
        Lit = 0,
        GBufferBaseColor,
        GBufferRoughnessMetalnessAo,
        GBufferNormals,
        GBufferGeometryNormals,
        GBufferDepth,
        GBufferVelocity,
        Clouds,
        Reflections,
        SSS,
        SSGI
    };

    class ViewportPanel : public Panel {

    public:
        ViewportPanel();

        void DrawMenuBar(std::function<void()> func);

        void DrawOverlay(std::function<void()> func);

        void Render(Ref<Scene::Scene>& scene, bool isActiveWindow);

        void RenderScene(Ref<Scene::Scene>& scene, ivec2 pos, ivec2 size, bool isActive);

        Ref<Viewport> viewport;
        Texture::Texture2D viewportTexture;

        PrimitiveBatchWrapper primitiveBatchWrapper;

        ViewportVisualization visualization = Lit;

        std::function<void()> drawMenuBarFunc;
        std::function<void()> drawOverlayFunc;

    private:
        void RenderVisualization();

        void CreateRenderPass();

        bool firstFrame = true;

        Ref<Graphics::RenderPass> renderPass;
        Ref<Graphics::FrameBuffer> frameBuffer;

    };

}
