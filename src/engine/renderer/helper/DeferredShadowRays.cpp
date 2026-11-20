#include "DeferredShadowRays.h"
#include "../../pipeline/PipelineManager.h"

namespace Atlas {

    namespace Renderer {

        namespace Helper {

            void DeferredShadowRays::Init() {

                pipelineConfig = PipelineConfig("raytracer/traceShadow.csh");

                sampleBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, 2 * sizeof(vec4));
                lightBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(vec4));
                rayBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, 2 * sizeof(vec4));
                rayCountBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBufferBit, sizeof(uint32_t), 1);

            }

            void DeferredShadowRays::Prepare(Graphics::CommandList* commandList, size_t sampleCount, size_t lightCount) {

                this->lightCount = lightCount;

                if (sampleBuffer.GetElementCount() != sampleCount)
                    sampleBuffer.SetSize(sampleCount);
                if (lightBuffer.GetElementCount() != lightCount) {
                    lightBuffer.SetSize(lightCount);
                    rayBuffer.SetSize(lightCount);
                }

                Graphics::BufferBarrier preBufferBarriers[] = {
                    { rayCountBuffer.Get(), VK_ACCESS_TRANSFER_WRITE_BIT }
                };
                commandList->PipelineBarrier({}, preBufferBarriers, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

                uint32_t zero = 0;
                commandList->FillBuffer(rayCountBuffer.Get(), &zero);

                Graphics::BufferBarrier postBufferBarriers[] = {
                    { rayCountBuffer.Get(), VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT }
                };
                commandList->PipelineBarrier({}, postBufferBarriers, VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

            }

            void DeferredShadowRays::BindForTrace(Graphics::CommandList* commandList) {

                commandList->BindBuffer(sampleBuffer.Get(), 3, 12);
                commandList->BindBuffer(lightBuffer.Get(), 3, 13);
                commandList->BindBuffer(rayBuffer.Get(), 3, 14);
                commandList->BindBuffer(rayCountBuffer.Get(), 3, 15);

            }

            void DeferredShadowRays::BindForResolve(Graphics::CommandList* commandList) {

                commandList->BindBuffer(sampleBuffer.Get(), 3, 12);
                commandList->BindBuffer(lightBuffer.Get(), 3, 13);

            }

            void DeferredShadowRays::Trace(Graphics::CommandList* commandList) {

                Graphics::BufferBarrier bufferBarriers[] = {
                    { sampleBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
                    { lightBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
                    { rayBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT },
                    { rayCountBuffer.Get(), VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT }
                };
                commandList->PipelineBarrier({}, bufferBarriers);

                auto pipeline = PipelineManager::GetPipeline(pipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->BindBuffer(lightBuffer.Get(), 3, 13);
                commandList->BindBuffer(rayBuffer.Get(), 3, 14);
                commandList->BindBuffer(rayCountBuffer.Get(), 3, 15);

                const uint32_t groupSize = 32;
                auto groupCount = uint32_t(lightCount) / groupSize;
                groupCount += (uint32_t(lightCount) % groupSize == 0) ? 0 : 1;

                commandList->Dispatch(groupCount, 1, 1);

                commandList->BufferMemoryBarrier(lightBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);

            }

        }

    }

}
