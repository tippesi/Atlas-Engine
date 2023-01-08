#include "DDGIRenderer.h"

#include "../common/RandomHelper.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

        void DDGIRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);
			Helper::GeometryHelper::GenerateSphereVertexArray(sphereArray, 10, 10);

			rayHitBuffer = Buffer::Buffer(Buffer::BufferUsageBits::StorageBuffer, sizeof(vec4));
            rayGenUniformBuffer = Buffer::Buffer(Buffer::BufferUsageBits::UniformBuffer |
                Buffer::BufferUsageBits::HostAccess | Buffer::BufferUsageBits::MultiBuffered,
                sizeof(RayGenUniforms), 1);
            rayHitUniformBuffer = Buffer::Buffer(Buffer::BufferUsageBits::UniformBuffer |
                Buffer::BufferUsageBits::HostAccess | Buffer::BufferUsageBits::MultiBuffered,
                sizeof(RayHitUniforms), 1);

            rayGenPipelineConfig = PipelineConfig("ddgi/rayGen.csh");
            rayHitPipelineConfig = PipelineConfig("ddgi/rayHit.csh");

            probeStatePipelineConfig = PipelineConfig("ddgi/probeState.csh");
            probeIrradianceUpdatePipelineConfig = PipelineConfig("ddgi/probeUpdate.csh", {"IRRADIANCE"});
            probeMomentsUpdatePipelineConfig = PipelineConfig("ddgi/probeUpdate.csh");
            irradianceCopyEdgePipelineConfig = PipelineConfig("ddgi/copyEdge.csh", {"IRRADIANCE"});
            momentsCopyEdgePipelineConfig = PipelineConfig("ddgi/copyEdge.csh");

            auto samplerDesc = Graphics::SamplerDesc {
                .filter = VK_FILTER_LINEAR,
                .mode = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                .compareEnabled = true
            };
            shadowSampler = device->CreateSampler(samplerDesc);

            PipelineManager::AddPipeline(rayGenPipelineConfig);
            PipelineManager::AddPipeline(rayHitPipelineConfig);
            PipelineManager::AddPipeline(probeStatePipelineConfig);
            PipelineManager::AddPipeline(probeIrradianceUpdatePipelineConfig);
            PipelineManager::AddPipeline(probeMomentsUpdatePipelineConfig);
            PipelineManager::AddPipeline(irradianceCopyEdgePipelineConfig);
            PipelineManager::AddPipeline(momentsCopyEdgePipelineConfig);

            /*
			probeDebugShader.AddStage(AE_VERTEX_STAGE, "ddgi/probeDebug.vsh");
			probeDebugShader.AddStage(AE_FRAGMENT_STAGE, "ddgi/probeDebug.fsh");
			probeDebugShader.Compile();
            */

		}

		void DDGIRenderer::TraceAndUpdateProbes(Scene::Scene* scene, Graphics::CommandList* commandList) {

			auto volume = scene->irradianceVolume;
			if (!volume || !volume->enable || !volume->update)
				return;

			Graphics::Profiler::BeginQuery("DDGI");

			// Try to get a shadow map
			Lighting::Shadow* shadow = nullptr;
			if (!scene->sky.sun) {
				auto lights = scene->GetLights();
				for (auto& light : lights) {
					if (light->type == AE_DIRECTIONAL_LIGHT) {
						shadow = light->GetShadow();
					}
				}
			}
			else {
				shadow = scene->sky.sun->GetShadow();
			}

			rayHitPipelineConfig.ManageMacro("USE_SHADOW_MAP", shadow && volume->useShadowMap);
			
			auto& internalVolume = volume->internal;
			internalVolume.SwapTextures();

			auto totalProbeCount = volume->probeCount.x *
				volume->probeCount.y *
				volume->probeCount.z;

			auto rayCount = volume->rayCount;
			auto rayCountInactive = volume->rayCountInactive;

			auto totalRayCount = rayCount * totalProbeCount;

			Graphics::Profiler::BeginQuery("Scene update");

			if (totalRayCount != rayHitBuffer.GetElementCount()) {
				helper.SetRayBufferSize(totalRayCount);
				rayHitBuffer.SetSize(totalRayCount);
				helper.SetScene(scene, 8);
			}

			auto [irradianceArray, momentsArray] = internalVolume.GetCurrentProbes();
			auto [lastIrradianceArray, lastMomentsArray] = internalVolume.GetLastProbes();

			auto& rayDirBuffer = internalVolume.rayDirBuffer;
			auto& rayDirInactiveBuffer = internalVolume.rayDirInactiveBuffer;
			auto& probeStateBuffer = internalVolume.probeStateBuffer;
			auto& probeOffsetBuffer = internalVolume.probeOffsetBuffer;

			helper.SetScene(scene, 8, volume->sampleEmissives);
			helper.UpdateLights();

            commandList->BindBuffer(probeStateBuffer.Get(), 2, 19);
            commandList->BindBuffer(probeOffsetBuffer.Get(), 2, 20);

			Graphics::Profiler::EndAndBeginQuery("Ray generation");

            auto rayGenPipeline = PipelineManager::GetPipeline(rayGenPipelineConfig);
			helper.DispatchRayGen(commandList, rayGenPipeline, volume->probeCount, false,
				[&]() {
					using namespace Common;

					auto theta = acosf(2.0f * float(rand()) / float(RAND_MAX) - 1.0f);
					auto phi = glm::two_pi<float>() * float(rand()) / float(RAND_MAX);

					auto dir = glm::vec3(
						2.0f * Random::SampleUniformFloat() - 1.0f,
						2.0f * Random::SampleUniformFloat() - 1.0f,
						2.0f * Random::SampleUniformFloat() - 1.0f
					);

					auto epsilon = glm::two_pi<float>() * float(rand()) / float(RAND_MAX);
					auto rot = mat3(glm::rotate(epsilon, dir));

                    auto uniforms = RayGenUniforms {
                        .rotationMatrix = mat4(rot)
                    };
                    rayGenUniformBuffer.SetData(&uniforms, 0, 1);

                    commandList->BindBuffer(rayDirBuffer.Get(), 3, 0);
                    commandList->BindBuffer(rayDirInactiveBuffer.Get(), 3, 1);
                    commandList->BindBuffer(rayGenUniformBuffer.GetMultiBuffer(), 3, 2);
				}
			);

            Graphics::Profiler::EndAndBeginQuery("Ray evaluation");

            commandList->BindImage(lastIrradianceArray.image, lastIrradianceArray.sampler, 2, 24);
            commandList->BindImage(lastMomentsArray.image, lastMomentsArray.sampler, 2, 25);

            auto rayHitPipeline = PipelineManager::GetPipeline(rayHitPipelineConfig);
			helper.DispatchHitClosest(commandList, rayHitPipeline, false,
				[&]() {
                    RayHitUniforms uniforms;
                    uniforms.seed = Common::Random::SampleUniformFloat();

                    if (shadow && volume->useShadowMap) {
                        auto &shadowUniform = uniforms.shadow;
                        shadowUniform.distance = !shadow->longRange ? shadow->distance : shadow->longRangeDistance;
                        shadowUniform.bias = shadow->bias;
                        shadowUniform.cascadeBlendDistance = shadow->cascadeBlendDistance;
                        shadowUniform.cascadeCount = shadow->componentCount;
                        shadowUniform.resolution = vec2(shadow->resolution);

                        commandList->BindImage(shadow->maps.image, shadowSampler, 3, 0);

                        auto componentCount = shadow->componentCount;
                        for (int32_t i = 0; i < MAX_SHADOW_CASCADE_COUNT + 1; i++) {
                            if (i < componentCount) {
                                auto cascade = &shadow->components[i];
                                auto frustum = Volume::Frustum(cascade->frustumMatrix);
                                auto corners = frustum.GetCorners();
                                auto texelSize = glm::max(abs(corners[0].x - corners[1].x),
                                    abs(corners[1].y - corners[3].y)) / (float)shadow->resolution;
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                                shadowUniform.cascades[i].cascadeSpace = cascade->projectionMatrix *
                                    cascade->viewMatrix;
                                shadowUniform.cascades[i].texelSize = texelSize;
                            } else {
                                auto cascade = &shadow->components[componentCount - 1];
                                shadowUniform.cascades[i].distance = cascade->farDistance;
                            }
                        }
                    }
                    rayHitUniformBuffer.SetData(&uniforms, 0, 1);

					// Use this buffer instead of the default writeRays buffer of the helper
                    commandList->BindBuffer(rayHitBuffer.Get(), 3, 1);
                    commandList->BindBuffer(rayHitUniformBuffer.GetMultiBuffer(), 3, 2);
				}
			);

            commandList->BufferMemoryBarrier(rayHitBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);
            commandList->BindBuffer(rayHitBuffer.Get(), 3, 1);

            commandList->ImageMemoryBarrier(irradianceArray.image, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            commandList->ImageMemoryBarrier(momentsArray.image, VK_IMAGE_LAYOUT_GENERAL,
                VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

            Graphics::Profiler::EndAndBeginQuery("Update probes");

			ivec3 probeCount = volume->probeCount;

			// Update the probes
			{
                Graphics::Profiler::BeginQuery("Update irradiance");

                commandList->BindImage(irradianceArray.image, 3, 0);

                auto pipeline = PipelineManager::GetPipeline(probeIrradianceUpdatePipelineConfig);
                commandList->BindPipeline(pipeline);

				commandList->Dispatch(probeCount.x, probeCount.y, probeCount.z);

                Graphics::Profiler::EndAndBeginQuery("Update moments");

                commandList->BindImage(momentsArray.image, 3, 0);

                pipeline = PipelineManager::GetPipeline(probeMomentsUpdatePipelineConfig);
                commandList->BindPipeline(pipeline);

				commandList->Dispatch(probeCount.x, probeCount.y, probeCount.z);

                Graphics::Profiler::EndQuery();
			}

            Graphics::Profiler::EndAndBeginQuery("Update probe states");

			// Update the states of the probes
			{
                auto pipeline = PipelineManager::GetPipeline(probeStatePipelineConfig);
                commandList->BindPipeline(pipeline);

                commandList->Dispatch(probeCount.x, probeCount.y, probeCount.z);
			}

            Graphics::Profiler::EndAndBeginQuery("Update probe edges");

			// Copy the probe edges
			{
                commandList->ImageMemoryBarrier(irradianceArray.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                auto pipeline = PipelineManager::GetPipeline(irradianceCopyEdgePipelineConfig);
                commandList->BindPipeline(pipeline);

                auto probeRes = volume->irrRes;
                auto constantRange = pipeline->shader->GetPushConstantRange("constants");

                auto res = ivec2(irradianceArray.width, irradianceArray.height);
                auto groupCount = res / 8;

                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                commandList->PushConstants(constantRange, &probeRes);
                commandList->BindImage(irradianceArray.image, 3, 0);
                commandList->Dispatch(groupCount.x, groupCount.y, probeCount.z);

                commandList->ImageMemoryBarrier(momentsArray.image, VK_IMAGE_LAYOUT_GENERAL,
                    VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

                pipeline = PipelineManager::GetPipeline(momentsCopyEdgePipelineConfig);
                commandList->BindPipeline(pipeline);

                probeRes = volume->momRes;
                constantRange = pipeline->shader->GetPushConstantRange("constants");

                res = ivec2(momentsArray.width, momentsArray.height);
                groupCount = res / 8;

                groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
                groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

                commandList->PushConstants(constantRange, &probeRes);
                commandList->BindImage(momentsArray.image, 3, 0);
                commandList->Dispatch(groupCount.x, groupCount.y, probeCount.z);

			}

			helper.InvalidateRayBuffer(commandList);

            commandList->BufferMemoryBarrier(probeStateBuffer.Get(), VK_ACCESS_SHADER_READ_BIT);
            commandList->ImageMemoryBarrier(irradianceArray.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT);
            commandList->ImageMemoryBarrier(momentsArray.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_ACCESS_SHADER_READ_BIT);

            commandList->BindImage(irradianceArray.image, irradianceArray.sampler, 2, 24);
            commandList->BindImage(momentsArray.image, momentsArray.sampler, 2, 25);

            Graphics::Profiler::EndQuery();
            Graphics::Profiler::EndQuery();

		}

		void DDGIRenderer::DebugProbes(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene, std::unordered_map<void*, uint16_t>& materialMap) {

            /*
			auto volume = scene->irradianceVolume;
			if (!volume || !volume->enable || !volume->update || !volume->debug)
				return;

			glDisable(GL_CULL_FACE);
            
			probeDebugActiveMaterial.emissiveColor = vec3(0.0f, 1.0f, 0.0f);
			probeDebugInactiveMaterial.emissiveColor = vec3(1.0f, 0.0f, 0.0f);
			probeDebugOffsetMaterial.emissiveColor = vec3(0.0f, 0.0f, 1.0f);

			sphereArray.Bind();
			probeDebugShader.Bind();

			volume->internal.probeStateBuffer.BindBase(9);

			probeDebugShader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
			probeDebugShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
			probeDebugShader.GetUniform("pvMatrixLast")->SetValue(camera->GetLastJitteredMatrix());

			probeDebugShader.GetUniform("jitterLast")->SetValue(camera->GetLastJitter());
			probeDebugShader.GetUniform("jitterCurrent")->SetValue(camera->GetJitter());

			probeDebugShader.GetUniform("probeMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugMaterial]));
			probeDebugShader.GetUniform("probeActiveMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugActiveMaterial]));
			probeDebugShader.GetUniform("probeInactiveMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugInactiveMaterial]));
			probeDebugShader.GetUniform("probeOffsetMaterialIdx")->SetValue(uint32_t(materialMap[&probeDebugOffsetMaterial]));

			probeDebugShader.GetUniform("volumeMin")->SetValue(volume->aabb.min);
			probeDebugShader.GetUniform("volumeMax")->SetValue(volume->aabb.max);
			probeDebugShader.GetUniform("volumeProbeCount")->SetValue(volume->probeCount);

			probeDebugShader.GetUniform("cellSize")->SetValue(volume->cellSize);
			probeDebugShader.GetUniform("cellLength")->SetValue(glm::length(volume->cellSize));

			auto [irradianceArray, momentsArray] = volume->internal.GetCurrentProbes();
			irradianceArray.Bind(12);
			momentsArray.Bind(13);

			volume->internal.probeStateBuffer.BindBase(9);
			volume->internal.probeOffsetBuffer.BindBase(10);

			auto instanceCount = volume->probeCount.x * volume->probeCount.y * volume->probeCount.z;
			glDrawElementsInstanced(GL_TRIANGLES, (int32_t)sphereArray.GetIndexComponent()->GetElementCount(),
				sphereArray.GetIndexComponent()->GetDataType(), nullptr, instanceCount);

			glEnable(GL_CULL_FACE);
             */

		}

	}

}