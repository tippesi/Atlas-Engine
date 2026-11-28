#pragma once

#include "../System.h"
#include "../pipeline/PipelineConfig.h"
#include "../graphics/CommandList.h"
#include "../buffer/UniformBuffer.h"

#include <vector>
#include <unordered_map>
#include <mutex>

namespace Atlas {

    namespace Ocean {

        struct OceanSurface {
            vec3 position = vec3(0.0f);
            vec3 normal = vec3(0.0f, 1.0f, 0.0f);
        };

        // Surfaces of the waves above or below positions, computed on the GPU and valid a few frames later
        class OceanSurfaceQueries {

        public:
            OceanSurfaceQueries();

            uint32_t CreateQuery(const std::vector<vec3>& positions);

            // Returns false if the query doesn't exist (anymore), the results stay until the new ones arrive
            bool UpdateQuery(uint32_t queryId, const std::vector<vec3>& positions);

            // Using a query in any way keeps it alive
            bool IsQueryValid(uint32_t queryId);

            std::vector<OceanSurface> GetQueryResults(uint32_t queryId);

            void DestroyQuery(uint32_t queryId);

            // Removes the queries that weren't used for queryLifetime updates
            void Update();

            // Needs the ocean uniforms and textures to be bound, the terrain shore line as well if there is one
            void Compute(Graphics::CommandList* commandList, bool shoreLine);

            int32_t queryLifetime = 8;

        private:
            struct Query {
                std::vector<vec3> positions;
                std::vector<OceanSurface> results;
                int32_t unusedUpdates = 0;
            };

            struct QueryRange {
                uint32_t queryId;
                size_t offset;
                size_t count;
            };

            struct alignas(16) Uniforms {
                int32_t count;
            };

            // Scripts use the queries while a render thread computes them, the pointer keeps the ocean movable
            std::unique_ptr<std::mutex> mutex = std::make_unique<std::mutex>();
            std::unordered_map<uint32_t, Query> queries;
            uint32_t queryCounter = 0;

            PipelineConfig queryConfig;
            Buffer::UniformBuffer uniformBuffer;

            Ref<Graphics::MultiBuffer> positionBuffer = nullptr;
            Ref<Graphics::MultiBuffer> resultBuffer = nullptr;
            size_t capacity = 0;

            // The queries that each frame copy of the buffers computes
            std::vector<QueryRange> ranges[FRAME_DATA_COUNT];
            std::vector<vec4> positions[FRAME_DATA_COUNT];
            int32_t frameIndex = -1;

        };

    }

}
