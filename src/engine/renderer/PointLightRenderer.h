#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class PointLightRenderer : public Renderer {

        public:
            PointLightRenderer();

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

        private:
            void GetUniforms();

            Buffer::VertexArray vertexArray;

            /*
            OldShader::OldShader shader;

            OldShader::Uniform* viewMatrix = nullptr;
            OldShader::Uniform* projectionMatrix = nullptr;
            OldShader::Uniform* inverseProjectionMatrix = nullptr;
            OldShader::Uniform* lightViewMatrix = nullptr;
            OldShader::Uniform* lightProjectionMatrix = nullptr;
            OldShader::Uniform* viewSpaceLightLocation = nullptr;
            OldShader::Uniform* lightLocation = nullptr;
            OldShader::Uniform* lightColor = nullptr;
            OldShader::Uniform* lightAmbient = nullptr;
            OldShader::Uniform* lightRadius = nullptr;
            OldShader::Uniform* shadowEnabled = nullptr;
            */

        };

    }

}