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

                void SetScene(Scene::Scene* scene, int32_t textureDownscale = 1,
                    bool useEmissivesAsLights = false);

                void SetRayBufferSize(size_t rayCount);

                void DispatchAndHit(Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& dispatchAndHitPipeline,
                    glm::ivec3 dimensions, std::function<void(void)> prepare);

                void DispatchRayGen(Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& rayGenPipeline, glm::ivec3 dimensions,
                    bool binning, std::function<void(void)> prepare);

                void DispatchHitClosest(Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& hitPipeline, bool binning,
                    bool opacityCheck, std::function<void(void)> prepare);

                void DispatchHitAny(Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& hitPipeline, std::function<void(void)> prepare);

                void DispatchGather(Graphics::CommandList* commandList,
                    const Ref<Graphics::Pipeline>& gatherPipeline, std::function<void(void)> prepare);

                void InvalidateRayBuffer(Graphics::CommandList* commandList);

                Buffer::Buffer* GetRayBuffer();

                void UpdateLights();


            private:
                struct alignas(16) PushConstants {
                    int32_t lightCount;
                    uint32_t rayBufferOffset;
                    uint32_t rayPayloadBufferOffset;
                    uint32_t rayBufferSize;
                    int32_t useRayBinning;
                };

                Scene::Scene* scene;
                
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