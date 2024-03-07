#pragma once

#include "Renderer.h"
#include <fsr2/ffx-fsr2-api/ffx_fsr2.h>

namespace Atlas::Renderer {

    class FSR2Renderer : public Renderer {

    public:
        FSR2Renderer() = default;

        ~FSR2Renderer();

        void Init(Graphics::GraphicsDevice* device);

        void Render(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, Graphics::CommandList* commandList);

        vec2 GetJitter(const Ref<RenderTarget>& target, uint32_t index) const;

    private:
        bool CheckContextNeedsUpdate() const;

        void CreateContext(const Ref<RenderTarget>& target, Graphics::GraphicsDevice* device);

        void DestroyContext();

        void GenerateShaders();

        FfxFsr2ContextDescription initParams = {};
        FfxFsr2Context context;

        std::vector<Ref<Graphics::Shader>> shaders;

    };

}