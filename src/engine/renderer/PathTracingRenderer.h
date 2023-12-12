#pragma once

#include "../System.h"
#include "Renderer.h"
#include "helper/RayTracingHelper.h"

#include "../texture/TextureAtlas.h"

#include <unordered_map>

namespace Atlas {

    namespace Renderer {

        class PathTracerRenderTarget {

        public:
            PathTracerRenderTarget() {}

            PathTracerRenderTarget(int32_t width, int32_t height) : width(width), height(height) {
                texture = Texture::Texture2D(width, height, VK_FORMAT_R8G8B8A8_UNORM,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

                frameAccumTexture = Texture::Texture2DArray(width, height, 3, VK_FORMAT_R32_UINT);

                accumTexture0 = Texture::Texture2D(width, height, VK_FORMAT_R32G32B32A32_SFLOAT);
                accumTexture1 = Texture::Texture2D(width, height, VK_FORMAT_R32G32B32A32_SFLOAT);
            }

            void Resize(int32_t width, int32_t height) {
                this->width = width;
                this->height = height;

                texture.Resize(width, height);

                accumTexture0.Resize(width, height);
                accumTexture1.Resize(width, height);
            }

            int32_t GetWidth() const { return width; }
            int32_t GetHeight() const { return height; }

            Texture::Texture2D texture;

            Texture::Texture2DArray frameAccumTexture;

            Texture::Texture2D accumTexture0;
            Texture::Texture2D accumTexture1;

            int32_t sampleCount = 0;

        private:
            int32_t width = 0;
            int32_t height = 0;

        };

        class PathTracingRenderer : public Renderer {

        public:
            PathTracingRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene);

            void Render(Viewport* viewport, PathTracerRenderTarget* renderTarget,
                ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene, Graphics::CommandList* commandList);

            bool UpdateData(Scene::Scene* scene);

            void UpdateMaterials(Scene::Scene* scene);

            void ResetSampleCount();

            int32_t GetSampleCount() const;

            int32_t bounces = 10;
            int32_t bvhDepth = 0;
            int32_t lightCount = 512;

            bool realTime = true;
            int32_t realTimeSamplesPerFrame = 1;

            float maxRadiance = 65535.0f;

        private:
            struct alignas(16) RayGenUniforms {
                vec4 origin;
                vec4 right;
                vec4 bottom;
                ivec2 pixelOffset;
                ivec2 resolution;
                ivec2 tileSize;
                int32_t sampleCount;
            };

            struct alignas(16) RayHitUniforms {
                ivec2 resolution;
                int32_t maxBounces;
                int32_t sampleCount;
                int32_t bounceCount;
                float seed;
                float exposure;
                int32_t samplesPerFrame;
                float maxRadiance;
            };

            Helper::RayTracingHelper helper;

            vec3 cameraLocation;
            vec2 cameraRotation;

            int32_t sampleCount = 0;
            ivec2 imageOffset = ivec2(0);

            PipelineConfig rayGenPipelineConfig;
            PipelineConfig rayHitPipelineConfig;

            Buffer::UniformBuffer rayGenUniformBuffer;
            Buffer::UniformBuffer rayHitUniformBuffer;

            size_t frameCount = 0;

        };

    }

}