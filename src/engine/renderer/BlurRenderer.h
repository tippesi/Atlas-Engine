#pragma once

#include "../System.h"
#include "Renderer.h"

namespace Atlas {

    namespace Renderer {

        class BlurRenderer : Renderer {

        public:
            BlurRenderer(std::string vertexSource, std::string fragmentSource, int32_t channelCount, float* kernelOffsets,
                         float* kernelWeights, int32_t kernelSize, bool bilateral = false);

            void Render(Texture::Texture2D* texture, Texture::Texture2D* swapTexture, Texture::Texture2D* depthTexture);

        private:
            void GetUniforms();

            float* kernelOffsets = nullptr;
            float* kernelWeights = nullptr;
            int32_t kernelSize;

            bool bilateralBlur = true;

            //OldShader::OldShader shader;

            //OldShader::Uniform* blurDirection = nullptr;


        };


    }

}