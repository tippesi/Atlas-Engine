#pragma once

#include "../System.h"
#include "../graphics/GraphicsDevice.h"

#include "RenderBatch.h"

#include "OpaqueRenderer.h"
#include "ImpostorRenderer.h"
#include "TerrainRenderer.h"
#include "OceanRenderer.h"
#include "ShadowRenderer.h"
#include "ImpostorShadowRenderer.h"
#include "TerrainShadowRenderer.h"
#include "DecalRenderer.h"
#include "DirectLightRenderer.h"
#include "PointLightRenderer.h"
#include "IndirectLightRenderer.h"
#include "TemporalAARenderer.h"
#include "SkyboxRenderer.h"
#include "AtmosphereRenderer.h"
#include "PostProcessRenderer.h"
#include "GBufferDownscaleRenderer.h"
#include "TextRenderer.h"
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

            /**
             *
             * @param window
             * @param target
             * @param camera
             * @param scene
             */
            void RenderScene(Viewport* viewport, RenderTarget* target, Camera* camera,
                Scene::Scene* scene, Texture::Texture2D* texture = nullptr, 
                RenderBatch* batch = nullptr);

            /**
             *
             * @param window
             * @param target
             * @param camera
             * @param scene
             */
            void PathTraceScene(Viewport* viewport, PathTracerRenderTarget* target, Camera* camera,
                Scene::Scene* scene, Texture::Texture2D* texture = nullptr);

            /**
             *
             * @param window
             * @param color
             * @param x
             * @param y
             * @param width
             * @param height
             * @param alphaBlending
             * @param framebuffer
             */
            void RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
                                 bool alphaBlending = false);

            /**
             *
             * @param window
             * @param color
             * @param x
             * @param y
             * @param width
             * @param height
             * @param clipArea
             * @param blendArea
             * @param alphaBlending
             * @param framebuffer
             */
            void RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
                                 vec4 clipArea, vec4 blendArea, bool alphaBlending = false);

            /**
             *
             * @param viewport
             * @param camera
             * @param batch
             */
            void RenderBatched(Viewport* viewport, Camera* camera, RenderBatch* batch);

            /**
             * Renders the scene into an environment probe
             * @param probe The environment probe.
             * @param target A render target to support rendering
             * @param scene The scene which should be rendered-
             * @note The render target must have the same resolution as the probe.
             */
            void RenderProbe(Lighting::EnvironmentProbe* probe, RenderTarget* target, Scene::Scene* scene);

            /**
             * Filters an environment probe.
             * @param probe The environment probe.
             * @note A probe has to be filtered to support image based lighting
             */
            void FilterProbe(Lighting::EnvironmentProbe* probe, Graphics::CommandList* commandList);

            /**
             * Update of the renderer
             * @warning Must be called every frame
             */
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

            void SetUniforms(Scene::Scene* scene, Camera* camera);

            void PrepareMaterials(Scene::Scene* scene, std::vector<PackedMaterial>& materials,
                std::unordered_map<void*, uint16_t>& materialMap);

            void FillRenderList(Scene::Scene* scene, Camera* camera);

            void PreintegrateBRDF();

            Graphics::GraphicsDevice* device = nullptr;

            Texture::Texture2D dfgPreintegrationTexture;

            Ref<Graphics::MultiBuffer> globalUniformBuffer;
            Ref<Graphics::MultiBuffer> pathTraceGlobalUniformBuffer;
            Ref<Graphics::MultiBuffer> ddgiUniformBuffer;

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