#pragma once

#include "Common.h"
#include "Image.h"

#include "Viewport.h"
#include "Font.h"
#include "renderer/MainRenderer.h"
#include "texture/Texture2D.h"
#include "graphics/RenderPass.h"
#include "graphics/Framebuffer.h"

#include <map>

namespace Atlas::Capture {

    // Renders a scene into an offscreen texture like the editor viewport does,
    // optionally renders debug views of intermediate passes and reads everything back
    class CaptureRenderer {

    public:
        struct TextLine {

            std::string text;
            vec4 color = vec4(1.0f);

        };

        void Init(Ref<Renderer::MainRenderer> mainRenderer, ivec2 resolution);

        // Changes the resolution of an initialized renderer, the render target is resized such that
        // renderers which keep state per target (e.g. FSR2) adapt to it
        void Resize(ivec2 resolution);

        void Render(const Ref<Scene::Scene>& scene, const std::vector<std::string>& passes,
            std::span<Ref<Renderer::PrimitiveBatch>> primitiveBatches = {}, bool pathTrace = false);

        // Shows the last rendered frame in the (possibly hidden) window. This also keeps
        // the swap chain acquire/present cycle going. Text lines are shown in a panel below the frame
        void Present(const Ref<Font>& font = nullptr, const std::vector<TextLine>& lines = {});

        // Height of the text panel for a number of lines
        static float GetPanelHeight(const Ref<Font>& font, size_t lineCount);

        // Only valid after the frame that rendered the content has finished on the GPU
        Ref<Image> ReadOutput();

        Ref<Image> ReadPass(const std::string& pass);

        static const std::vector<std::string>& GetPassNames();

        const Ref<Viewport>& GetViewport() const { return viewport; }

        // Scaling factor of the render target, the output keeps the resolution
        void SetRenderScale(float renderScale);

        float GetRenderScale() const;

        // Temporal effects start over, e.g. when switching to another camera while the scene is paused
        void InvalidateHistory() { renderTarget->InvalidateHistory(); }

        ivec2 resolution = ivec2(0);

    private:
        struct PassTarget {

            Ref<Texture::Texture2D> texture;
            Ref<Graphics::FrameBuffer> frameBuffer;

        };

        void RenderPass(const std::string& pass, float exposure);

        PassTarget& GetPassTarget(const std::string& pass);

        Ref<Renderer::MainRenderer> mainRenderer;
        Ref<Renderer::RenderTarget> renderTarget;
        Ref<Viewport> viewport;

        Ref<Texture::Texture2D> outputTexture;

        Ref<Graphics::RenderPass> passRenderPass;
        std::map<std::string, PassTarget> passTargets;

        bool hasFrame = false;

        static constexpr float panelPadding = 8.0f;

    };

}
