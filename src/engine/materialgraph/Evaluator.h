#pragma once

#include "MaterialGraph.h"

#include "../Material.h"
#include "../buffer/Buffer.h"
#include "../buffer/UniformBuffer.h"
#include "../buffer/VertexArray.h"
#include "../graphics/CommandList.h"
#include "../mesh/Mesh.h"
#include "../pipeline/PipelineConfig.h"

#include <vector>

namespace Atlas::MaterialGraph {

    /**
     * Resources for rendering shaders of the Compiler outside of the regular passes, which have their own
     * global data and only see the textures of the evaluated graph.
     */
    class Evaluator {

    public:
        struct PushConstants {
            float time = 0.0f;
            uint32_t invertUVs = 0;
        };

        Evaluator();

        // Applies the values of a material, or the defaults of the graph without one
        void UpdateParameters(const std::vector<Parameter>& parameters, uint32_t parameterWords, const Material* material);

        void Bind(Graphics::CommandList* commandList);

        // Without a mesh the shader draws a plane
        PipelineConfig GetPipelineConfig(const EvaluationShader& shader, const Ref<Graphics::FrameBuffer>& frameBuffer, Mesh::Mesh* mesh);

        Buffer::VertexArray planeVertexArray;

    private:
        Buffer::Buffer parameterBuffer;
        Buffer::UniformBuffer globalBuffer;
        Ref<Graphics::Sampler> sampler;

        // In the order of DefaultTexture
        std::vector<Ref<Texture::Texture2D>> defaultTextures;
        std::vector<Ref<Graphics::Image>> images;
        std::vector<uint32_t> words;

    };

}
