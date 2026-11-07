#pragma once

#include "materialgraph/Evaluator.h"
#include "materialgraph/MaterialGraph.h"
#include "texture/Texture2D.h"

#include <imgui.h>

#include <unordered_map>
#include <vector>

namespace Atlas::Editor::UI {

    // Small pictures of what nodes calculate, all in one texture. Shaders are compiled in the background for
    // nodes in view, only nodes depending on the time render again regularly
    class MaterialGraphPreviews {

    public:
        // The graph changed, all previews need to be generated again
        void Invalidate();

        // Another graph is shown, its node ids mean something else
        void Reset();

        // Requests the preview of a node for this frame
        void Request(uint32_t nodeId);

        void Render(const MaterialGraph::Graph& graph, const Material* material);

        // False while there is no picture of the node yet
        bool GetImage(uint32_t nodeId, ImTextureID& texture, ImVec2& uvMin, ImVec2& uvMax) const;

    private:
        struct Entry {
            int32_t slot = -1;
            int32_t revision = -1;
            bool rendered = false;
            bool failed = false;
            MaterialGraph::EvaluationShader shader;
            PipelineConfig config;
        };

        void Init();

        ImVec2 GetSlotMin(int32_t slot) const;

        Scope<MaterialGraph::Evaluator> evaluator;

        Ref<Texture::Texture2D> atlas;
        Ref<Graphics::RenderPass> renderPass;
        Ref<Graphics::FrameBuffer> frameBuffer;
        bool isAtlasPrepared = false;

        std::unordered_map<uint32_t, Entry> entries;
        std::vector<uint32_t> requested;

        int32_t revision = 0;
        int32_t nextSlot = 0;
        int32_t frame = 0;
        Hash lastParameters = 0;

        static constexpr int32_t slotSize = 128;
        static constexpr int32_t slotsPerRow = 16;
        static constexpr int32_t maxGeneratedPerFrame = 6;
        // Animated previews don't have to follow every frame
        static constexpr int32_t animationInterval = 3;

    };

}
