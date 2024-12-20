#pragma once

#include "../System.h"
#include "../volume/AABB.h"
#include "../texture/Texture2DArray.h"
#include "../buffer/Buffer.h"
#include "../buffer/UniformBuffer.h"

namespace Atlas {

    namespace Renderer {
        class MainRenderer;
    }

    namespace Mesh {

        struct ImpostorViewPlane {
            vec4 right;
            vec4 up;
        };

        class Impostor {

            friend Renderer::MainRenderer;

        public:
            Impostor();

            Impostor(int32_t views, int32_t resolution);

            void FillViewPlaneBuffer(const std::vector<vec3>& rightVectors, const std::vector<vec3>& upVectors);

            void RefreshViewPlaneBuffer();

            void AllocateTextures();

            Ref<Texture::Texture2DArray> baseColorTexture;
            Ref<Texture::Texture2DArray> roughnessMetalnessAoTexture;
            Ref<Texture::Texture2DArray> normalTexture;
            Ref<Texture::Texture2DArray> depthTexture;

            Buffer::Buffer viewPlaneBuffer;
            Buffer::UniformBuffer impostorInfoBuffer;

            std::vector<ImpostorViewPlane> viewPlanes;

            vec3 center = vec3(0.0f);
            float radius = 1.0f;

            int32_t views = 1;
            int32_t resolution = 64;

            float cutoff = 0.7f;
            float mipBias = -1.0f;

            bool interpolation = false;
            bool pixelDepthOffset = true;

            vec3 approxTransmissiveColor = vec3(0.0f);
            float approxReflectance = 0.5f;

            bool isGenerated = false;

        private:
            struct ImpostorInfo {
                vec4 center;

                float radius;
                int32_t views;

                float cutoff;
                float mipBias;
            };

        };

    }

}