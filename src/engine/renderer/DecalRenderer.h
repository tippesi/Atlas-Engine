#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class DecalRenderer : public Renderer {

        public:
            DecalRenderer();

            void Render(Ref<RenderTarget> target, Ref<Scene::Scene> scene);

            static std::string vertexPath;
            static std::string fragmentPath;

        private:
            void GetUniforms();

            Buffer::VertexArray vertexArray;

            /*
            OldShader::OldShader shader;

            OldShader::Uniform* modelMatrix = nullptr;
            OldShader::Uniform* viewMatrix = nullptr;
            OldShader::Uniform* projectionMatrix = nullptr;
            OldShader::Uniform* inverseViewMatrix = nullptr;
            OldShader::Uniform* inverseProjectionMatrix = nullptr;

            OldShader::Uniform* color = nullptr;

            OldShader::Uniform* timeInMilliseconds = nullptr;
            OldShader::Uniform* animationLength = nullptr;
            OldShader::Uniform* rowCount = nullptr;
            OldShader::Uniform* columnCount = nullptr;
             */

        };

    }

}