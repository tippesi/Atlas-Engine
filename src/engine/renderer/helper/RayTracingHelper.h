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

                void DispatchAndHit(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& dispatchAndHitPipeline,
                    glm::ivec3 dimensions, std::function<void(void)> prepare);

                void DispatchRayGen(Ref<Scene::Scene> scene, Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& rayGenPipeline, glm::ivec3 dimensions,
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


            private:
                struct alignas(16) PushConstants {
                    int32_t lightCount;
                    uint32_t rayBufferOffset;
                    uint32_t rayPayloadBufferOffset;
                    uint32_t rayBufferSize;
                    int32_t useRayBinning;
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