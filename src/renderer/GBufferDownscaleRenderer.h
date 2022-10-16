#ifndef AE_GBUFFERDOWNSCALERENDERER_H
#define AE_GBUFFERDOWNSCALERENDERER_H

#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class GBufferDownscaleRenderer : public Renderer {

        public:
            GBufferDownscaleRenderer();

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

            void Downscale(RenderTarget* target);

            void DownscaleDepthOnly(RenderTarget* target);

        private:
            void Downscale(Texture::Texture2D* depthIn, Texture::Texture2D* normalIn,
                Texture::Texture2D* roughnessMetallicAoIn, Texture::Texture2D* depthOut,
                Texture::Texture2D* normalOut, Texture::Texture2D* roughnessMetallicAoOut,
                Texture::Texture2D* offsetOut);

            Shader::Shader downscale;
            Shader::Shader downscaleDepthOnly;

        };

    }

}

#endif
