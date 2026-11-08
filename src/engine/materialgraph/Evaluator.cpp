#include "Evaluator.h"

#include "../common/ColorConverter.h"
#include "../graphics/GraphicsDevice.h"
#include "../renderer/helper/CommonStructures.h"
#include "../renderer/helper/GeometryHelper.h"

#include <array>
#include <cstring>

namespace Atlas::MaterialGraph {

    Evaluator::Evaluator() : parameterBuffer(Buffer::BufferUsageBits::StorageBufferBit | Buffer::BufferUsageBits::HostAccessBit |
        Buffer::BufferUsageBits::MultiBufferedBit, sizeof(uint32_t), 1), globalBuffer(sizeof(Renderer::GlobalUniforms)) {

        Renderer::Helper::GeometryHelper::GenerateRectangleVertexArray(planeVertexArray);

        auto device = Graphics::GraphicsDevice::DefaultDevice;

        sampler = device->CreateSampler(Graphics::SamplerDesc{
            .filter = VK_FILTER_LINEAR,
            .mode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .maxLod = 12,
            .anisotropicFiltering = true
            });

        const std::vector<std::array<uint8_t, 4>> colors = { { 255, 255, 255, 255 }, { 0, 0, 0, 255 }, { 128, 128, 255, 255 } };
        for (const auto& color : colors) {
            auto texture = CreateRef<Texture::Texture2D>(1, 1, VK_FORMAT_R8G8B8A8_UNORM);
            std::vector<uint8_t> data(color.begin(), color.end());
            texture->SetData(data);
            defaultTextures.push_back(texture);
        }

        UpdateParameters({}, 0, nullptr);

    }

    void Evaluator::UpdateParameters(const std::vector<Parameter>& parameters, uint32_t parameterWords, const Material* material) {

        words.assign(std::max(parameterWords, 1u), 0u);

        images.clear();
        for (const auto& texture : defaultTextures)
            images.push_back(texture->image);

        for (const auto& parameter : parameters) {
            auto target = words.data() + parameter.offset;

            if (parameter.type == ValueType::Texture) {
                const auto& texture = material ? material->GetGraphTexture(parameter) : parameter.defaultTexture;

                target[0] = uint32_t(parameter.fallback);
                if (texture.IsLoaded()) {
                    target[0] = uint32_t(images.size());
                    images.push_back(texture->image);
                }
                continue;
            }

            parameter.WriteValue(material ? material->GetGraphValue(parameter) : parameter.defaultValue, target);
        }

        // Buffers exist once per frame in flight, so this has to be written for every frame which draws
        Renderer::GlobalUniforms uniforms = {};
        uniforms.vMatrix = mat4(1.0f);
        uniforms.ivMatrix = mat4(1.0f);
        uniforms.pMatrix = mat4(1.0f);
        uniforms.ipMatrix = mat4(1.0f);
        globalBuffer.SetData(&uniforms, 0);

        if (words.size() > parameterBuffer.GetElementCount())
            parameterBuffer.SetSize(words.size());
        parameterBuffer.SetData(words.data(), 0, words.size());

    }

    void Evaluator::Bind(Graphics::CommandList* commandList) {

        globalBuffer.Bind(commandList, 1, 31);
        parameterBuffer.Bind(commandList, 3, 9);
        commandList->BindSampler(sampler, 1, 14);
        commandList->BindSampledImages(images, 0, 3);

    }

    PipelineConfig Evaluator::GetPipelineConfig(const EvaluationShader& shader, const Graphics::AttachmentFormats& attachmentFormats,
        Mesh::Mesh* mesh) {

        auto shaderConfig = ShaderConfig{
            { "materialGraph/evaluate.vsh", VK_SHADER_STAGE_VERTEX_BIT },
            { shader.fragmentFile, VK_SHADER_STAGE_FRAGMENT_BIT },
        };

        auto pipelineDesc = Graphics::GraphicsPipelineDesc{
            .attachmentFormats = attachmentFormats,
            .vertexInputInfo = mesh ? mesh->vertexArray.GetVertexInputState() : planeVertexArray.GetVertexInputState(),
            .depthStencilInputInfo = Graphics::Initializers::InitPipelineDepthStencilStateCreateInfo(false, false, VK_COMPARE_OP_ALWAYS),
        };

        pipelineDesc.rasterizer.cullMode = VK_CULL_MODE_NONE;

        std::vector<std::string> macros;
        if (mesh) {
            macros.push_back("EVALUATE_MESH");
            if (mesh->data.tangents.ContainsData())
                macros.push_back("TANGENTS");
            if (mesh->data.colors.ContainsData())
                macros.push_back("VERTEX_COLORS");
        }
        else {
            pipelineDesc.assemblyInputInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        }

        return PipelineConfig(shaderConfig, pipelineDesc, macros);

    }

}
