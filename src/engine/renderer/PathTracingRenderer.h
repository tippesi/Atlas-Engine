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

                albedoTexture = Texture::Texture2D(width, height, VK_FORMAT_R8G8B8A8_UNORM);

                velocityTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);
                historyVelocityTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);

                depthTexture = Texture::Texture2D(width, height, VK_FORMAT_R32_SFLOAT);
                historyDepthTexture = Texture::Texture2D(width, height, VK_FORMAT_R32_SFLOAT);

                normalTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);
                historyNormalTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16_SFLOAT);

                materialIdxTexture = Texture::Texture2D(width, height, VK_FORMAT_R16_UINT);
                historyMaterialIdxTexture = Texture::Texture2D(width, height, VK_FORMAT_R16_UINT);

                radianceTexture = Texture::Texture2D(width, height, VK_FORMAT_R32G32B32A32_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
                historyRadianceTexture = Texture::Texture2D(width, height, VK_FORMAT_R32G32B32A32_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);

                postProcessTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
                historyPostProcessTexture = Texture::Texture2D(width, height, VK_FORMAT_R16G16B16A16_SFLOAT,
                    Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            }

            void Resize(int32_t width, int32_t height) {
                this->width = width;
                this->height = height;

                texture.Resize(width, height);

                frameAccumTexture.Resize(width, height, 3);

                albedoTexture.Resize(width, height);

                velocityTexture.Resize(width, height);
                historyVelocityTexture.Resize(width, height);

                depthTexture.Resize(width, height);
                historyDepthTexture.Resize(width, height);

                normalTexture.Resize(width, height);
                historyNormalTexture.Resize(width, height);

                materialIdxTexture.Resize(width, height);
                historyMaterialIdxTexture.Resize(width, height);

                radianceTexture.Resize(width, height);
                historyRadianceTexture.Resize(width, height);

                postProcessTexture.Resize(width, height);
                historyPostProcessTexture.Resize(width, height);
            }

            void Swap() {

                std::swap(radianceTexture, historyRadianceTexture);
                std::swap(velocityTexture, historyVelocityTexture);
                std::swap(depthTexture, historyDepthTexture);
                std::swap(normalTexture, historyNormalTexture);
                std::swap(materialIdxTexture, historyMaterialIdxTexture);
                std::swap(postProcessTexture, historyPostProcessTexture);

            }

            int32_t GetWidth() const { return width; }
            int32_t GetHeight() const { return height; }

            Texture::Texture2D texture;

            Texture::Texture2DArray frameAccumTexture;

            Texture::Texture2D albedoTexture;
            Texture::Texture2D velocityTexture;
            Texture::Texture2D depthTexture;
            Texture::Texture2D normalTexture;
            Texture::Texture2D materialIdxTexture;

            Texture::Texture2D historyVelocityTexture;
            Texture::Texture2D historyDepthTexture;
            Texture::Texture2D historyNormalTexture;
            Texture::Texture2D historyMaterialIdxTexture;

            Texture::Texture2D radianceTexture;
            Texture::Texture2D historyRadianceTexture;

            Texture::Texture2D postProcessTexture;
            Texture::Texture2D historyPostProcessTexture;

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
            int32_t lightCount = 128;

            bool realTime = true;
            int32_t realTimeSamplesPerFrame = 4;

            int32_t historyLengthMax = 32;
            float historyClipMax = 0.1f;
            float currentClipFactor = 2.0f;

            float maxRadiance = 65535.0f;

            bool sampleEmissives = false;

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
                int32_t frameCount;
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