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
#include "GBufferRenderer.h"
#include "TextRenderer.h"
#include "SSGIRenderer.h"
#include "DDGIRenderer.h"
#include "RTGIRenderer.h"
#include "AORenderer.h"
#include "RTReflectionRenderer.h"
#include "SSSRenderer.h"
#include "VolumetricRenderer.h"
#include "VolumetricCloudRenderer.h"
#include "VegetationRenderer.h"
#include "TextureRenderer.h"
#include "PathTracingRenderer.h"
#include "FSR2Renderer.h"

namespace Atlas {

    namespace Renderer {

        class MainRenderer {

        public:
            MainRenderer() = default;

            void Init(Graphics::GraphicsDevice* device);

            void RenderScene(Ref<Viewport> viewport, Ref<RenderTarget> target, Ref<Scene::Scene> scene,
                Ref<PrimitiveBatch> primitiveBatch = nullptr, Texture::Texture2D* texture = nullptr);

            void PathTraceScene(Ref<Viewport> viewport, Ref<PathTracerRenderTarget> target,
                Ref<Scene::Scene> scene, Texture::Texture2D* texture = nullptr);

            void RenderPrimitiveBatch(Ref<Viewport> viewport, Ref<RenderTarget> target,
                Ref<PrimitiveBatch> batch, const CameraComponent& camera, Graphics::CommandList* commandList = nullptr);

            void RenderProbe(Ref<Lighting::EnvironmentProbe> probe, Ref<RenderTarget> target, Ref<Scene::Scene> scene);

            void FilterProbe(Ref<Lighting::EnvironmentProbe> probe, Graphics::CommandList* commandList);

            void Update();

            TextRenderer textRenderer;
            TextureRenderer textureRenderer;
            OceanRenderer oceanRenderer;
            AtmosphereRenderer atmosphereRenderer;
            PathTracingRenderer pathTracingRenderer;

            Ref<Font> font;

        private:
            void CreateGlobalDescriptorSetLayout();

            void SetUniforms(const Ref<RenderTarget>& target, const Ref<Scene::Scene>& scene, const CameraComponent& camera);

            void PrepareMaterials(Ref<Scene::Scene> scene, std::vector<PackedMaterial>& materials,
                std::unordered_map<void*, uint16_t>& materialMap);

            void PrepareBindlessData(Ref<Scene::Scene> scene, std::vector<Ref<Graphics::Image>>& images,
                std::vector<Ref<Graphics::Buffer>>& blasBuffers, std::vector<Ref<Graphics::Buffer>>& triangleBuffers,
                std::vector<Ref<Graphics::Buffer>>& bvhTriangleBuffers, std::vector<Ref<Graphics::Buffer>>& triangleOffsetBuffers);

            void FillRenderList(Ref<Scene::Scene> scene, const CameraComponent& camera);

            void PreintegrateBRDF();

            PipelineConfig GetPipelineConfigForPrimitives(Ref<Graphics::FrameBuffer>& frameBuffer,
                Buffer::VertexArray& vertexArray, VkPrimitiveTopology topology, bool testDepth);

            Graphics::GraphicsDevice* device = nullptr;

            Texture::Texture2D dfgPreintegrationTexture;

            Ref<Graphics::MultiBuffer> globalUniformBuffer;
            Ref<Graphics::MultiBuffer> pathTraceGlobalUniformBuffer;
            Ref<Graphics::MultiBuffer> ddgiUniformBuffer;
            Ref<Graphics::MultiBuffer> lightUniformBuffer;
            Ref<Graphics::DescriptorSetLayout> globalDescriptorSetLayout;
            Ref<Graphics::Sampler> globalSampler;
            Ref<Graphics::Sampler> globalNearestSampler;

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
            GBufferRenderer gBufferRenderer;
            SSGIRenderer ssgiRenderer;
            DDGIRenderer ddgiRenderer;
            RTGIRenderer rtgiRenderer;
            AORenderer aoRenderer;
            RTReflectionRenderer rtrRenderer;
            SSSRenderer sssRenderer;
            VolumetricRenderer volumetricRenderer;
            VolumetricCloudRenderer volumetricCloudRenderer;
            FSR2Renderer fsr2Renderer;

            RenderList renderList;

            std::vector<vec2> haltonSequence;
            size_t haltonIndex = 0;
            uint32_t frameCount = 0;

        };

    }

}