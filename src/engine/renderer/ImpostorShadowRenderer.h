#ifndef AE_IMPOSTORSHADOWRENDERER_H
#define AE_IMPOSTORSHADOWRENDERER_H

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class ImpostorShadowRenderer : public Renderer {

        public:
            ImpostorShadowRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, RenderList* renderList,
                mat4 viewMatrix, mat4 projectionMatrix, vec3 location);

        private:
            void GetUniforms();

            Buffer::VertexArray vertexArray;

            /*
            OldShader::OldShader shader;

            OldShader::Uniform* vMatrix = nullptr;
            OldShader::Uniform* pMatrix = nullptr;
            OldShader::Uniform* cameraLocation = nullptr;

            OldShader::Uniform* center = nullptr;
            OldShader::Uniform* radius = nullptr;

            OldShader::Uniform* views = nullptr;
            OldShader::Uniform* cutoff = nullptr;
             */

        };

    }

}

#endif