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

        // Of the views of an impostor, see ImpostorRenderer::Generate
        struct alignas(16) ImpostorPushConstants {
            mat4 vMatrix = mat4(1.0f);
            float time = 0.0f;
            uint32_t invertUVs = 0;
            float distanceToPlaneCenter = 1.0f;
            uint32_t twoSided = 0;
        };

        Evaluator();

        // Compiles the whole surface of the graph of the material and applies the values of the material, whose textures
        // should be loaded in full resolution. Invalid without errors if the material has no graph which writes the surface
        EvaluationShader PrepareBake(const Material& material);

        // Applies the values of a material, or the defaults of the graph without one
        void UpdateParameters(const std::vector<Parameter>& parameters, uint32_t parameterWords, const Material* material);

        void Bind(Graphics::CommandList* commandList);

        // Without a mesh the shader draws a plane. With the material of an impostor the mesh is drawn from the views of
        // the impostor into its maps, otherwise it is laid out by its texture coordinates
        PipelineConfig GetPipelineConfig(const EvaluationShader& shader, const Graphics::AttachmentFormats& attachmentFormats,
            Mesh::Mesh* mesh, const Material* impostorMaterial = nullptr);

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
