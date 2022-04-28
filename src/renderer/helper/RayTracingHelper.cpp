#include "RayTracingHelper.h"
#include "../../common/RandomHelper.h"
#include "../../common/Piecewise.h"
#include "../../volume/BVH.h"
#include "../../Profiler.h"

#define DIRECTIONAL_LIGHT 0
#define TRIANGLE_LIGHT 1

namespace Atlas {

	namespace Renderer {

		namespace Helper {

			RayTracingHelper::RayTracingHelper() {

				const size_t lightCount = 512;

				glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureUnitCount);
				glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupLimit);

				indirectDispatchBuffer = Buffer::Buffer(AE_DISPATCH_INDIRECT_BUFFER, 3 *
					sizeof(uint32_t), 0);
				counterBuffer0 = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
					sizeof(uint32_t), 0);
				counterBuffer1 = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
					sizeof(uint32_t), 0);

				indirectDispatchBuffer.SetSize(1);
				counterBuffer0.SetSize(1);
				counterBuffer1.SetSize(1);

				// Create dynamic resizable shader storage buffers
				lightBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(Scene::RTData::GPULight),
					AE_BUFFER_DYNAMIC_STORAGE, lightCount);
				rayBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, 2 * sizeof(vec4),
					AE_BUFFER_DYNAMIC_STORAGE);
				rayPayloadBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(vec4),
					AE_BUFFER_DYNAMIC_STORAGE);
				rayBinCounterBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(uint32_t),
					AE_BUFFER_DYNAMIC_STORAGE);
				rayBinOffsetBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(uint32_t),
					AE_BUFFER_DYNAMIC_STORAGE);

				rayBinCounterBuffer.SetSize(64);
				rayBinOffsetBuffer.SetSize(64);

				traceDispatchShader.AddStage(AE_COMPUTE_STAGE, "raytracer/traceDispatch.csh");
				traceDispatchShader.Compile();

				traceClosestShader.AddStage(AE_COMPUTE_STAGE, "raytracer/traceClosest.csh");
				traceClosestShader.Compile();

				traceAnyShader.AddStage(AE_COMPUTE_STAGE, "raytracer/traceAny.csh");
				traceAnyShader.Compile();

				binningShader.AddStage(AE_COMPUTE_STAGE, "raytracer/binning.csh");
				binningShader.Compile();

				binningOffsetShader.AddStage(AE_COMPUTE_STAGE, "raytracer/binningOffset.csh");
				binningOffsetShader.Compile();

			}


			void RayTracingHelper::SetScene(Scene::Scene* scene, int32_t textureDownscale, 
				bool useEmissivesAsLights) {

				this->scene = scene;
				this->textureDownscale = textureDownscale;
				this->useEmissivesAsLights = useEmissivesAsLights;

			}

			void RayTracingHelper::SetRayBufferSize(size_t size) {

				// We use double buffering
				rayBuffer.SetSize(size * 2);
				rayPayloadBuffer.SetSize(size * 2);

			}

			void RayTracingHelper::DispatchAndHit(Shader::Shader* dispatchAndHitShader, glm::ivec3 dimensions, std::function<void(void)> prepare) {

				// Select lights once per initial ray dispatch
				{
					auto lightCount = lightBuffer.GetElementCount();
					selectedLights.clear();
					// Randomly select lights (only at image offset 0)
					if (lights.size() > 0) {
						std::vector<float> weights;
						weights.reserve(lights.size());
						for (auto& light : lights) {
							weights.push_back(light.data1.y);
						}

						auto piecewiseDistribution = Common::Piecewise1D(weights);

						for (size_t i = 0; i < lightCount; i++) {							
							float pdf = 0.0f;
							int32_t offset = 0;
							piecewiseDistribution.Sample(pdf, offset);

							selectedLights.push_back(lights[offset]);
						}

						for (auto& light : selectedLights) {
							light.data1.y *= float(selectedLights.size());
						}
					}
					else {
						for (auto light : lights) {
							light.data1.y = 1.0f;
							selectedLights.push_back(light);
						}
					}

					lightBuffer.SetData(selectedLights.data(), 0, selectedLights.size());
				}

				auto& rtData = scene->rayTracingData;

				// Bind textures and buffers
				{
					if (rtData.baseColorTextureAtlas.slices.size())
						rtData.baseColorTextureAtlas.texture.Bind(GL_TEXTURE0);
					if (rtData.opacityTextureAtlas.slices.size())
						rtData.opacityTextureAtlas.texture.Bind(GL_TEXTURE1);
					if (rtData.normalTextureAtlas.slices.size())
						rtData.normalTextureAtlas.texture.Bind(GL_TEXTURE2);
					if (rtData.roughnessTextureAtlas.slices.size())
						rtData.roughnessTextureAtlas.texture.Bind(GL_TEXTURE3);
					if (rtData.metalnessTextureAtlas.slices.size())
						rtData.metalnessTextureAtlas.texture.Bind(GL_TEXTURE4);
					if (rtData.aoTextureAtlas.slices.size())
						rtData.aoTextureAtlas.texture.Bind(GL_TEXTURE5);
					if (scene->sky.probe)
						scene->sky.probe->cubemap.Bind(GL_TEXTURE6);

					rtData.materialBuffer.BindBase(5);
					rtData.triangleBuffer.BindBase(6);
					rtData.nodeBuffer.BindBase(7);
					lightBuffer.BindBase(8);
				}

				// Execute shader
				{
					dispatchAndHitShader->Bind();
					dispatchAndHitShader->GetUniform("lightCount")->SetValue(int32_t(selectedLights.size()));

					prepare();

					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
						GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
					glDispatchCompute(dimensions.x, dimensions.y, dimensions.z);
				}

			}

			void RayTracingHelper::DispatchRayGen(Shader::Shader* rayGenShader, glm::ivec3 dimensions, 
				bool binning, std::function<void(void)> prepare) {

				dispatchCounter = 0;

				// Select lights once per initial ray dispatch
				{
					auto lightCount = lightBuffer.GetElementCount();
					selectedLights.clear();
					// Randomly select lights (only at image offset 0)
					if (lights.size() > lightCount) {
						std::vector<float> weights;
						weights.reserve(lights.size());
						for (auto& light : lights) {
							weights.push_back(light.data1.y);
						}

						auto piecewiseDistribution = Common::Piecewise1D(weights);

						for (size_t i = 0; i < lightCount; i++) {
							float pdf = 0.0f;
							int32_t offset = 0;
							piecewiseDistribution.Sample(pdf, offset);

							selectedLights.push_back(lights[offset]);
						}

						for (auto& light : selectedLights) {
							light.data1.y *= float(selectedLights.size());
						}
					}
					else {
						for (auto light : lights) {
							light.data1.y = 1.0f;
							selectedLights.push_back(light);
						}
					}

					lightBuffer.SetData(selectedLights.data(), 0, selectedLights.size());
				}

				rayGenShader->Bind();

				counterBuffer0.BindBase(1);
				counterBuffer1.BindBase(0);

				rayBuffer.BindBase(2);
				rayPayloadBuffer.BindBase(3);

				rayBinCounterBuffer.BindBase(11);

				rayGenShader->GetUniform("lightCount")->SetValue(int32_t(selectedLights.size()));
				rayGenShader->GetUniform("rayBufferOffset")->SetValue(uint32_t(1));
				rayGenShader->GetUniform("rayBufferSize")->SetValue(uint32_t(rayBuffer.GetElementCount() / 2));
				rayGenShader->GetUniform("useRayBinning")->SetValue(binning);

				prepare();

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
					GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
				glDispatchCompute(dimensions.x, dimensions.y, dimensions.z);

			}

			void RayTracingHelper::DispatchHitClosest(Shader::Shader* hitShader, 
				bool binning, std::function<void(void)> prepare) {

				auto& rtData = scene->rayTracingData;

				// Bind textures and buffers
				{
					if (rtData.baseColorTextureAtlas.slices.size())
						rtData.baseColorTextureAtlas.texture.Bind(GL_TEXTURE0);
					if (rtData.opacityTextureAtlas.slices.size())
						rtData.opacityTextureAtlas.texture.Bind(GL_TEXTURE1);
					if (rtData.normalTextureAtlas.slices.size())
						rtData.normalTextureAtlas.texture.Bind(GL_TEXTURE2);
					if (rtData.roughnessTextureAtlas.slices.size())
						rtData.roughnessTextureAtlas.texture.Bind(GL_TEXTURE3);
					if (rtData.metalnessTextureAtlas.slices.size())
						rtData.metalnessTextureAtlas.texture.Bind(GL_TEXTURE4);
					if (rtData.aoTextureAtlas.slices.size())
						rtData.aoTextureAtlas.texture.Bind(GL_TEXTURE5);
					if (scene->sky.probe)
						scene->sky.probe->cubemap.Bind(GL_TEXTURE6);

					rtData.materialBuffer.BindBase(5);
					rtData.triangleBuffer.BindBase(6);
					rtData.nodeBuffer.BindBase(7);
					lightBuffer.BindBase(8);
				}

				Profiler::BeginQuery("Setup command buffer");

				// Set up command buffer, reset ray count
				{
					indirectDispatchBuffer.BindBaseAs(AE_SHADER_STORAGE_BUFFER, 4);
					traceDispatchShader.Bind();

					if (dispatchCounter % 2 == 0) {
						counterBuffer0.BindBase(0);
						counterBuffer1.BindBase(1);
					}
					else {
						counterBuffer0.BindBase(1);
						counterBuffer1.BindBase(0);
					}

					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
					glDispatchCompute(1, 1, 1);
				}

				indirectDispatchBuffer.Bind();
				rayBuffer.BindBase(2);
				rayPayloadBuffer.BindBase(3);

				if (binning) {

					Profiler::EndAndBeginQuery("Binning offsets");

					// Calculate binning offsets
					{
						binningOffsetShader.Bind();

						rayBinCounterBuffer.BindBase(11);
						rayBinOffsetBuffer.BindBase(12);

						glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
						glDispatchCompute(1, 1, 1);
					}

					Profiler::EndAndBeginQuery("Binning rays");

					// Order rays by their bins
					{
						binningShader.Bind();

						binningShader.GetUniform("rayBufferOffset")->SetValue(uint32_t(dispatchCounter % 2));
						binningShader.GetUniform("rayBufferSize")->SetValue(uint32_t(rayBuffer.GetElementCount() / 2));

						glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
						glDispatchComputeIndirect(0);

						uint32_t zero = 0;
						rayBinCounterBuffer.Bind();
						rayBinCounterBuffer.InvalidateData();
						rayBinCounterBuffer.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);
					}

				}
				
				Profiler::EndAndBeginQuery("Trace rays");

				// Trace rays for closest intersection
				{
					traceClosestShader.Bind();

					traceClosestShader.GetUniform("rayBufferOffset")->SetValue(uint32_t(binning ? 1 - dispatchCounter % 2 : 0));
					traceClosestShader.GetUniform("rayBufferSize")->SetValue(uint32_t(rayBuffer.GetElementCount() / 2));

					glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
					glDispatchComputeIndirect(0);
				}

				Profiler::EndAndBeginQuery("Execute hit shader");
				
				// Shade rays
				{
					hitShader->Bind();

					hitShader->GetUniform("lightCount")->SetValue(int32_t(selectedLights.size()));
					hitShader->GetUniform("rayBufferOffset")->SetValue(uint32_t(binning ? dispatchCounter % 2 : 1));
					hitShader->GetUniform("rayPayloadBufferOffset")->SetValue(uint32_t(dispatchCounter) % 2);
					hitShader->GetUniform("rayBufferSize")->SetValue(uint32_t(rayBuffer.GetElementCount() / 2));
					hitShader->GetUniform("useRayBinning")->SetValue(binning);

					rayBinCounterBuffer.BindBase(11);

					prepare();

					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
						GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
					glDispatchComputeIndirect(0);
				}

				Profiler::EndQuery();

				dispatchCounter++;

			}

			void RayTracingHelper::InvalidateRayBuffer() {

				uint32_t zero = 0;
				counterBuffer0.Bind();
				counterBuffer0.InvalidateData();
				counterBuffer0.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);

				counterBuffer1.Bind();
				counterBuffer1.InvalidateData();
				counterBuffer1.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);

			}

			Buffer::Buffer* RayTracingHelper::GetRayBuffer() {

				// This is the latest write buffer in all tracing functions
				return &rayBuffer;

			}

			void RayTracingHelper::UpdateLights() {

				lights.clear();
				auto& rtData = scene->rayTracingData;

				auto lightSources = scene->GetLights();

				for (auto light : lightSources) {

					auto radiance = light->color * light->intensity;
					auto brightness = dot(radiance, vec3(0.3333f));

					vec3 P = vec3(0.0f);
					vec3 N = vec3(0.0f);
					float weight = 0.0f;
					float area = 0.0f;

					uint32_t data = 0;

					// Parse individual light information based on type
					if (light->type == AE_DIRECTIONAL_LIGHT) {
						auto dirLight = static_cast<Lighting::DirectionalLight*>(light);
						data |= (DIRECTIONAL_LIGHT << 28u);
						weight = brightness;
						N = dirLight->direction;
					}
					else if (light->type == AE_POINT_LIGHT) {

					}

					data |= uint32_t(lights.size());
					auto cd = reinterpret_cast<float&>(data);

					Scene::RTData::GPULight gpuLight;
					gpuLight.data0 = vec4(P, radiance.r);
					gpuLight.data1 = vec4(cd, weight, area, radiance.g);
					gpuLight.N = vec4(N, radiance.b);

					lights.push_back(gpuLight);
				}

				if (useEmissivesAsLights)
					lights.insert(lights.end(), rtData.triangleLights.begin(), rtData.triangleLights.end());

				// Find the maximum weight
				auto maxWeight = 0.0f;
				for (auto& light : lights) {
					maxWeight = glm::max(maxWeight, light.data1.y);
				}

				// Calculate min weight and adjust lights based on it
				auto minWeight = 0.005f * maxWeight;
				// Also calculate the total weight
				auto totalWeight = 0.0f;

				for (auto& light : lights) {
					light.data1.y = glm::max(light.data1.y, minWeight);
					totalWeight += light.data1.y;
				}

				for (auto& light : lights) {
					light.data1.y /= totalWeight;
				}
			}
			

		}

	}

}