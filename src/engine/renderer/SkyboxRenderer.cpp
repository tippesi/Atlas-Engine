#include "SkyboxRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

        void SkyboxRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            Helper::GeometryHelper::GenerateCubeVertexArray(vertexArray);

        }

		void SkyboxRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
            Scene::Scene* scene, Graphics::CommandList* commandList) {

			Graphics::Profiler::BeginQuery("Skybox");

			vertexArray.Bind(commandList);


            auto pipelineConfig = GetPipelineConfig(target);
            auto pipeline = PipelineManager::GetPipeline(pipelineConfig);

            commandList->BindPipeline(pipeline);

            vec4 lastCameraLocation = vec4(camera->GetLastLocation(), 1.0f);
            auto constantRange = pipeline->shader->GetPushConstantRange("constants");
            commandList->PushConstants(constantRange, &lastCameraLocation);

            auto& cubemap = scene->sky.probe->cubemap;
            commandList->BindImage(cubemap.image, cubemap.sampler, 3, 0);

            commandList->Draw(36, 1, 0, 0);

            Graphics::Profiler::EndQuery();

		}

        PipelineConfig SkyboxRenderer::GetPipelineConfig(RenderTarget *target) {

            auto shaderConfig = ShaderConfig {
                { "skybox.vsh", VK_SHADER_STAGE_VERTEX_BIT },
                { "skybox.fsh", VK_SHADER_STAGE_FRAGMENT_BIT }
            };
            auto pipelineDesc = Graphics::GraphicsPipelineDesc {
                .frameBuffer = target->lightingFrameBuffer,
                .vertexInputInfo = vertexArray.GetVertexInputState(),
                .depthStencilInputInfo = Graphics::Initializers::InitPipelineDepthStencilStateCreateInfo(
                    true, false, VK_COMPARE_OP_LESS_OR_EQUAL),
                .rasterizer = Graphics::Initializers::InitPipelineRasterizationStateCreateInfo(
                    VK_POLYGON_MODE_FILL, VK_CULL_MODE_NONE)
            };
            return PipelineConfig(shaderConfig, pipelineDesc);

		}

	}

}