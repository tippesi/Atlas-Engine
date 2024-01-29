#pragma once

#include "../System.h"
#include "../graphics/GraphicsDevice.h"

#include "PrimitiveBatch.h"

#include "OpaqueRenderer.h"
#include "ImpostorRenderer.h"
#include "TerrainRenderer.h"
#include "OceanRenderer.h"
#include "ShadowRenderer.h"
#include "ImpostorShadowRenderer.h"
#include "TerrainShadowRenderer.h"
#include "DecalRenderer.h"
#include "DirectLightRenderer.h"
#include "IndirectLightRenderer.h"
#include "TemporalAARenderer.h"
#include "SkyboxRenderer.h"
#include "AtmosphereRenderer.h"
#include "PostProcessRenderer.h"
#include "GBufferDownscaleRenderer.h"
#include "TextRenderer.h"
#include "GIRenderer.h"
#include "DDGIRenderer.h"
#include "AORenderer.h"
#include "RTReflectionRenderer.h"
#include "SSSRenderer.h"
#include "VolumetricRenderer.h"
#include "VolumetricCloudRenderer.h"
#include "VegetationRenderer.h"
#include "TextureRenderer.h"
#include "PathTracingRenderer.h"

namespace Atlas {

    namespace Renderer {

        class MainRenderer {

        public:
            MainRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void RenderScene(Ref<Viewport> viewport, Ref<RenderTarget> target, Ref<Scene::Scene> scene,
                Ref<PrimitiveBatch> batch = nullptr, Texture::Texture2D* texture = nullptr);

            void PathTraceScene(Ref<Viewport> viewport, Ref<PathTracerRenderTarget> target,
                Ref<Scene::Scene> scene, Texture::Texture2D* texture = nullptr);

            void RenderBatched(Ref<Viewport> viewport, Ref<PrimitiveBatch> batch, const CameraComponent& camera);

            void RenderProbe(Ref<Lighting::EnvironmentProbe> probe, Ref<RenderTarget> target, Ref<Scene::Scene> scene);

            void FilterProbe(Ref<Lighting::EnvironmentProbe> probe, Graphics::CommandList* commandList);

            void Update();

            TextRenderer textRenderer;
            TextureRenderer textureRenderer;
            OceanRenderer oceanRenderer;
            AtmosphereRenderer atmosphereRenderer;
            PathTracingRenderer pathTracingRenderer;

        private:
            struct PackedMaterial {

                int32_t baseColor;
                int32_t emissiveColor;
                int32_t transmissionColor;

                uint32_t emissiveIntensityTiling;

                int32_t data0;
                int32_t data1;
                int32_t data2;

                int32_t features;

            };

            struct alignas(16) GlobalUniforms {
                vec4 frustumPlanes[6];
                mat4 vMatrix;
                mat4 pMatrix;
                mat4 ivMatrix;
                mat4 ipMatrix;
                mat4 pvMatrixLast;
                mat4 pvMatrixCurrent;
                vec2 jitterLast;
                vec2 jitterCurrent;
                vec4 cameraLocation;
                vec4 cameraDirection;
                vec4 cameraUp;
                vec4 cameraRight;
                vec4 planetCenter;
                vec2 windDir;
                float windSpeed;
                float planetRadius;
                float time;
                float deltaTime;
                uint32_t frameCount;
            };

            struct alignas(16) DDGIUniforms {
                vec4 volumeMin;
                vec4 volumeMax;
                ivec4 volumeProbeCount;
                vec4 cellSize;

                float volumeBias;

                int32_t volumeIrradianceRes;
                int32_t volumeMomentsRes;

                uint32_t rayCount;
                uint32_t inactiveRayCount;

                float hysteresis;

                float volumeGamma;
                float volumeStrength;

                float depthSharpness;
                int optimizeProbes;

                int32_t volumeEnabled;
            };

            void CreateGlobalDescriptorSetLayout();

            void SetUniforms(Ref<Scene::Scene> scene, const CameraComponent& camera);

            void PrepareMaterials(Ref<Scene::Scene> scene, std::vector<PackedMaterial>& materials,
                std::unordered_map<void*, uint16_t>& materialMap);

            void PrepareBindlessData(Ref<Scene::Scene> scene, std::vector<Ref<Graphics::Image>>& images,
                std::vector<Ref<Graphics::Buffer>>& blasBuffers, std::vector<Ref<Graphics::Buffer>>& triangleBuffers,
                std::vector<Ref<Graphics::Buffer>>& bvhTriangleBuffers, std::vector<Ref<Graphics::Buffer>>& triangleOffsetBuffers);

            void FillRenderList(Ref<Scene::Scene> scene, const CameraComponent& camera);

            void PreintegrateBRDF();

            Graphics::GraphicsDevice* device = nullptr;

            Texture::Texture2D dfgPreintegrationTexture;

            Ref<Graphics::MultiBuffer> globalUniformBuffer;
            Ref<Graphics::MultiBuffer> pathTraceGlobalUniformBuffer;
            Ref<Graphics::MultiBuffer> ddgiUniformBuffer;
            Ref<Graphics::DescriptorSetLayout> globalDescriptorSetLayout;
            Ref<Graphics::Sampler> globalSampler;

            Buffer::VertexArray vertexArray;
            Buffer::VertexArray cubeVertexArray;

            OpaqueRenderer opaqueRenderer;
            ImpostorRenderer impostorRenderer;
            TerrainRenderer terrainRenderer;
            ShadowRenderer shadowRenderer;
            ImpostorShadowRenderer impostorShadowRenderer;
            VegetationRenderer vegetationRenderer;
            TerrainShadowRenderer terrainShadowRenderer;
            DecalRenderer decalRenderer;
            DirectLightRenderer directLightRenderer;
            IndirectLightRenderer indirectLightRenderer;

            TemporalAARenderer taaRenderer;
            SkyboxRenderer skyboxRenderer;
            PostProcessRenderer postProcessRenderer;
            GBufferDownscaleRenderer downscaleRenderer;
            GIRenderer giRenderer;
            DDGIRenderer ddgiRenderer;
            AORenderer aoRenderer;
            RTReflectionRenderer rtrRenderer;
            SSSRenderer sssRenderer;
            VolumetricRenderer volumetricRenderer;
            VolumetricCloudRenderer volumetricCloudRenderer;

            RenderList renderList;

            std::vector<vec2> haltonSequence;
            size_t haltonIndex = 0;
            uint32_t frameCount = 0;

        };

    }

}