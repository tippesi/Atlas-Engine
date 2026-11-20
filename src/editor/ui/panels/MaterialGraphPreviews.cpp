#include "MaterialGraphPreviews.h"

#include "../../Singletons.h"

#include "Clock.h"
#include "graphics/GraphicsDevice.h"
#include "materialgraph/Compiler.h"
#include "materialgraph/PipelineCache.h"
#include "pipeline/PipelineManager.h"

namespace Atlas::Editor::UI {

    using namespace MaterialGraph;

    void MaterialGraphPreviews::Invalidate() {

        revision++;

    }

    void MaterialGraphPreviews::Reset() {

        entries.clear();
        requested.clear();
        nextSlot = 0;

    }

    void MaterialGraphPreviews::Request(uint32_t nodeId) {

        requested.push_back(nodeId);

    }

    void MaterialGraphPreviews::Init() {

        evaluator = std::make_unique<Evaluator>();

        auto size = slotSize * slotsPerRow;
        atlas = CreateRef<Texture::Texture2D>(size, size, VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge,
            Texture::Filtering::Linear, false, true);

        rendering = Graphics::RenderingDesc{ .colorAttachments = { { .image = atlas->image } } };

    }

    void MaterialGraphPreviews::Render(const Graph& graph, const Material* material) {


        if (requested.empty())
            return;

        if (!evaluator)
            Init();

        frame++;

        auto generated = 0;
        auto parameterShader = static_cast<const EvaluationShader*>(nullptr);

        for (auto nodeId : requested) {
            auto& entry = entries[nodeId];
            if (entry.slot < 0) {
                if (nextSlot >= slotsPerRow * slotsPerRow)
                    continue;
                entry.slot = nextSlot++;
            }

            if (entry.revision != revision && generated < maxGeneratedPerFrame) {
                generated++;
                entry.revision = revision;
                entry.shader = Compiler::CompilePreview(graph, nodeId);
                entry.failed = !entry.shader.valid;
                entry.rendered = false;
                if (!entry.failed)
                    entry.config = evaluator->GetPipelineConfig(entry.shader, Graphics::AttachmentFormats(rendering), nullptr);
            }

            if (entry.revision == revision && !entry.failed)
                parameterShader = &entry.shader;
        }

        if (!parameterShader) {
            requested.clear();
            return;
        }

        evaluator->UpdateParameters(parameterShader->parameters, parameterShader->parameterWords, material);

        // Values of the material changing render the previews again
        Hash parameters = 0;
        if (material) {
            for (const auto& [name, value] : material->graphValues) {
                HashCombine(parameters, name);
                HashCombine(parameters, value.x);
                HashCombine(parameters, value.y);
                HashCombine(parameters, value.z);
            }
            for (const auto& [name, texture] : material->graphTextures) {
                HashCombine(parameters, name);
                HashCombine(parameters, texture.IsLoaded() ? texture->image.get() : nullptr);
            }
        }
        // Textures finish loading in the background
        for (const auto& parameter : parameterShader->parameters)
            HashCombine(parameters, parameter.defaultTexture.IsLoaded() ? 1 : 0);

        auto parametersChanged = parameters != lastParameters;
        lastParameters = parameters;

        auto isAnimationFrame = frame % animationInterval == 0;

        auto device = Graphics::GraphicsDevice::DefaultDevice;
        Graphics::CommandList* commandList = nullptr;

        for (auto nodeId : requested) {
            auto& entry = entries[nodeId];
            if (entry.slot < 0 || entry.revision != revision || entry.failed)
                continue;

            if (entry.rendered && !parametersChanged && !(entry.shader.usesTime && isAnimationFrame))
                continue;

            if (PipelineCache::Request(entry.config, false) != PipelineCache::State::Ready)
                continue;

            if (!commandList) {
                commandList = device->GetCommandList(Graphics::GraphicsQueue);
                commandList->BeginCommands();

                if (!isAtlasPrepared) {
                    commandList->ImageMemoryBarrier(atlas->image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
                    isAtlasPrepared = true;
                }

                commandList->BeginRendering(rendering);
            }

            auto pipeline = PipelineManager::GetPipeline(entry.config);
            commandList->BindPipeline(pipeline);
            evaluator->planeVertexArray.Bind(commandList);
            evaluator->Bind(commandList);

            auto x = (entry.slot % slotsPerRow) * slotSize;
            auto y = (entry.slot / slotsPerRow) * slotSize;
            commandList->SetViewport(x, y, slotSize, slotSize);
            commandList->SetScissor(x, y, slotSize, slotSize);

            Evaluator::PushConstants constants = { .time = Clock::Get() };
            commandList->PushConstants("constants", &constants);

            commandList->Draw(4);

            entry.rendered = true;
        }

        if (commandList) {
            commandList->EndRendering();
            commandList->EndCommands();
            device->SubmitCommandList(commandList);
        }

        requested.clear();

    }

    bool MaterialGraphPreviews::GetImage(uint32_t nodeId, ImTextureID& texture, ImVec2& uvMin, ImVec2& uvMax) const {

        auto iter = entries.find(nodeId);
        if (iter == entries.end() || iter->second.slot < 0 || !isAtlasPrepared)
            return false;

        // A preview from before the last change is better than an empty node
        if (!iter->second.rendered)
            return false;

        const auto& slot = iter->second.slot;
        auto scale = 1.0f / float(slotsPerRow);
        uvMin = ImVec2(float(slot % slotsPerRow) * scale, float(slot / slotsPerRow) * scale);
        uvMax = ImVec2(uvMin.x + scale, uvMin.y + scale);
        texture = Singletons::imguiWrapper->GetTextureId(atlas.get());

        return true;

    }

}
