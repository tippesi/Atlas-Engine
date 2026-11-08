#pragma once

#include "Panel.h"
#include "tools/PrimitiveBatchWrapper.h"
#include "scene/Scene.h"
#include "renderer/target/RenderTarget.h"
#include "renderer/MainRenderer.h"
#include "Viewport.h"
#include "Icons.h"

namespace Atlas::Editor::UI {

    enum ViewportVisualization {
        Lit = 0,
        GBufferBaseColor,
        GBufferRoughnessMetalnessAo,
        GBufferEmissive,
        GBufferNormals,
        GBufferGeometryNormals,
        GBufferMaterialIdx,
        GBufferStencil,
        GBufferDepth,
        GBufferVelocity,
        Volumetrics,
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

        void Render(Ref<Scene::Scene>& scene);

        void RenderScene(Ref<Scene::Scene>& scene, ivec2 pos, ivec2 size);

        // Ground grid on the y = 0 plane around the camera
        void RenderGrid(const Scene::Components::CameraComponent& camera);

        // Toolbars are drawn as floating, rounded button groups on top of the viewport from the menu bar function
        float GetToolbarGroupWidth(int32_t buttonCount) const;

        void BeginToolbarGroup(float x, int32_t buttonCount);

        bool ToolbarButton(const char* id, IconType iconType, bool selected, bool enabled, const char* tooltip,
            ImVec2 uvMin = ImVec2(0.15f, 0.15f), ImVec2 uvMax = ImVec2(0.85f, 0.85f));

        // Contents of a "Visualization settings" popup, which selects the visualization
        void RenderVisualizationPopup();

        static constexpr float toolbarMargin = 8.0f;
        static constexpr float toolbarGroupPadding = 3.0f;
        static constexpr float toolbarButtonSpacing = 2.0f;
        static constexpr float toolbarFramePadding = 5.0f;

        // In ImGui units for the interface, the scene is rendered with the render viewport
        Ref<Viewport> viewport;
        Ref<Viewport> renderViewport;
        Texture::Texture2D viewportTexture;

        // All visible viewports are rendered in the same frame, so uniform buffers, upscalers and history can't be shared
        Ref<Renderer::RenderTarget> renderTarget;
        Ref<Renderer::MainRenderer> mainRenderer;

        // Off for content the path tracer can't show, e.g. material graphs
        bool allowPathTracing = true;

        PrimitiveBatchWrapper primitiveBatchWrapper;

        ViewportVisualization visualization = Lit;

        std::function<void()> drawMenuBarFunc;
        std::function<void()> drawOverlayFunc;

        // Frame around the viewport, shows e.g. that the scene is playing. Invisible with an alpha of zero
        ImVec4 frameColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    private:
        void RenderVisualization(float exposure);

        void CreateRendering();

        bool firstFrame = true;

        Graphics::RenderingDesc rendering;

    };

}
