#include "OceanSurfaceQueries.h"
#include "../graphics/GraphicsDevice.h"
#include "../graphics/Profiler.h"
#include "../pipeline/PipelineManager.h"

#include <bit>

namespace Atlas {

    namespace Ocean {

        OceanSurfaceQueries::OceanSurfaceQueries() {

            queryConfig = PipelineConfig("ocean/queries.csh");
            uniformBuffer = Buffer::UniformBuffer(sizeof(Uniforms));

        }

        uint32_t OceanSurfaceQueries::CreateQuery(const std::vector<vec3>& positions) {

            // The counter wraps around, ids that are still in use are skipped
            do {
                queryCounter++;
            } while (queryCounter == 0 || queries.contains(queryCounter));

            queries[queryCounter].positions = positions;

            return queryCounter;

        }

        bool OceanSurfaceQueries::UpdateQuery(uint32_t queryId, const std::vector<vec3>& positions) {

            auto it = queries.find(queryId);
            if (it == queries.end())
                return false;

            it->second.positions = positions;
            it->second.unusedUpdates = 0;

            return true;

        }

        bool OceanSurfaceQueries::IsQueryValid(uint32_t queryId) {

            auto it = queries.find(queryId);
            if (it == queries.end())
                return false;

            it->second.unusedUpdates = 0;

            return !it->second.results.empty();

        }

        std::vector<OceanSurface> OceanSurfaceQueries::GetQueryResults(uint32_t queryId) {

            auto it = queries.find(queryId);
            if (it == queries.end())
                return {};

            it->second.unusedUpdates = 0;

            return it->second.results;

        }

        void OceanSurfaceQueries::DestroyQuery(uint32_t queryId) {

            queries.erase(queryId);

        }

        void OceanSurfaceQueries::Update() {

            // Results arrive FRAME_DATA_COUNT frames later, queries need to live at least that long
            auto lifetime = std::max(queryLifetime, FRAME_DATA_COUNT + 1);
            std::erase_if(queries, [&](auto& item) {
                return ++item.second.unusedUpdates > lifetime;
                });

        }

        void OceanSurfaceQueries::Compute(Graphics::CommandList* commandList, bool shoreLine) {

            auto device = Graphics::GraphicsDevice::DefaultDevice;

            // A scene that is rendered multiple times in a frame computes its queries only once
            auto currentFrameIndex = device->GetFrameIndex();
            if (frameIndex == currentFrameIndex)
                return;
            frameIndex = currentFrameIndex;

            // The GPU finished the queries of this frame copy of the buffers FRAME_DATA_COUNT frames ago
            auto& frameRanges = ranges[frameIndex % FRAME_DATA_COUNT];
            auto& framePositions = positions[frameIndex % FRAME_DATA_COUNT];
            if (!frameRanges.empty()) {
                resultBuffer->Invalidate();
                auto results = static_cast<vec4*>(resultBuffer->Map());
                for (const auto& range : frameRanges) {
                    auto it = queries.find(range.queryId);
                    if (it == queries.end())
                        continue;

                    auto& queryResults = it->second.results;
                    queryResults.resize(range.count);
                    for (size_t i = 0; i < range.count; i++) {
                        auto result = results[range.offset + i];
                        auto position = framePositions[range.offset + i];
                        queryResults[i] = { vec3(position.x, result.w, position.z), vec3(result) };
                    }
                }
                resultBuffer->Unmap();
            }

            frameRanges.clear();
            framePositions.clear();
            for (const auto& [queryId, query] : queries) {
                if (query.positions.empty())
                    continue;

                frameRanges.push_back({ queryId, framePositions.size(), query.positions.size() });
                for (auto position : query.positions)
                    framePositions.push_back(vec4(position, 0.0f));
            }

            if (framePositions.empty())
                return;

            Graphics::Profiler::BeginQuery("Ocean surface queries");

            if (framePositions.size() > capacity) {
                capacity = std::max(std::bit_ceil(framePositions.size()), size_t(64));
                Graphics::BufferDesc desc = {
                    .usageFlags = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                    .domain = Graphics::BufferDomain::Host,
                    .size = capacity * sizeof(vec4)
                };
                positionBuffer = device->CreateMultiBuffer(desc);
                desc.hostAccess = Graphics::BufferHostAccess::Random;
                resultBuffer = device->CreateMultiBuffer(desc);

                // Queries that are still computed write into the old buffers
                for (int32_t i = 0; i < FRAME_DATA_COUNT; i++)
                    if (i != frameIndex % FRAME_DATA_COUNT)
                        ranges[i].clear();
            }

            positionBuffer->SetData(framePositions.data(), 0, framePositions.size() * sizeof(vec4));

            Uniforms uniforms = {
                .count = int32_t(framePositions.size())
            };
            uniformBuffer.SetData(&uniforms, 0);

            queryConfig.ManageMacro("TERRAIN", shoreLine);
            auto pipeline = PipelineManager::GetPipeline(queryConfig);
            commandList->BindPipeline(pipeline);

            uniformBuffer.Bind(commandList, 3, 17);
            commandList->BindBuffer(positionBuffer, 3, 14);
            commandList->BindBuffer(resultBuffer, 3, 18);

            commandList->Dispatch(int32_t((framePositions.size() + 63) / 64), 1, 1);

            commandList->MemoryBarrier(VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT);

            Graphics::Profiler::EndQuery();

        }

    }

}
