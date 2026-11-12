#pragma once

#include "../../System.h"
#include "../../scene/Scene.h"
#include "../../buffer/Buffer.h"
#include <functional>


namespace Atlas {

    namespace Renderer {

        namespace Helper {

            class RayTracingHelper {

            public:
                RayTracingHelper();

                void SetRayBufferSize(size_t rayCount);

                // Ray tracing pipeline of a ray generation stage with the shared miss and opacity stages
                static PipelineConfig GetRayTracingPipelineConfig(const std::string& rayGenFile);

                // The pipeline of a config with the closest hit stages of the material graphs of the scene. Until that
                // one is created in the background, or if it fails, the graphs use their baked values
                static Ref<Graphics::Pipeline> GetRayTracingPipeline(PipelineConfig& config, const Ref<Scene::Scene>& scene);

                // The dimensions are work groups for compute pipelines and invocations for ray tracing pipelines
                void DispatchAndHit(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& dispatchAndHitPipeline,
                    ivec3 dimensions, std::function<void(void)> prepare);

                void DispatchRayGen(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& rayGenPipeline, ivec3 dimensions,
                    bool binning, std::function<void(void)> prepare);

                void DispatchHitClosest(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& hitPipeline, bool binning,
                    bool opacityCheck, std::function<void(void)> prepare);

                void DispatchHitAny(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& hitPipeline, std::function<void(void)> prepare);

                void DispatchGather(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& gatherPipeline, std::function<void(void)> prepare);

                void InvalidateRayBuffer(Graphics::CommandList* commandList);

                Buffer::Buffer* GetRayBuffer();

                void UpdateLights(Ref<Scene::Scene> scene, bool useEmissivesAsLights = false);

                bool stochasticLightSelection = false;

            private:
                struct alignas(16) PushConstants {
                    int32_t lightCount = 0;
                    uint32_t rayBufferOffset = 0;
                    uint32_t rayPayloadBufferOffset = 0;
                    uint32_t rayBufferSize = 0;
                    int32_t useRayBinning = 0;
                    int32_t padding0 = 0;
                    int32_t padding1 = 0;
                    int32_t padding2 = 0;
                };
                
                std::vector<GPULight> lights;
                std::vector<GPULight> selectedLights;

                PipelineConfig traceDispatchPipelineConfig;
                PipelineConfig traceClosestPipelineConfig;
                PipelineConfig traceAnyPipelineConfig;

                PipelineConfig binningOffsetPipelineConfig;
                PipelineConfig binningPipelineConfig;

                Buffer::Buffer indirectDispatchBuffer;

                Buffer::Buffer counterBuffer0;
                Buffer::Buffer counterBuffer1;

                Buffer::Buffer rayBuffer;
                Buffer::Buffer rayPayloadBuffer;

                Buffer::Buffer rayBinCounterBuffer;
                Buffer::Buffer rayBinOffsetBuffer;
                
                Buffer::Buffer lightBuffer;
                Texture::Texture2DArray dummyTexture;

                int32_t dispatchCounter = 0;
                int32_t rayOffsetCounter = 0;
                int32_t payloadOffsetCounter = 0;

                int32_t textureDownscale;
                bool useEmissivesAsLights = false;

            };

        }

    }

}