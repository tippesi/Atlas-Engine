#include "VolumetricCloudRenderer.h"

#include "common/RandomHelper.h"
#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

        void VolumetricCloudRenderer::Init(Graphics::GraphicsDevice *device) {

            this->device = device;

            auto noiseImage = Loader::ImageLoader::LoadImage<uint8_t>("noise.png");
            blueNoiseTexture = Texture::Texture2D(noiseImage.width, noiseImage.height, VK_FORMAT_R8G8B8A8_UNORM);
            blueNoiseTexture.SetData(noiseImage.GetData());

            shapeNoisePipelineConfig = PipelineConfig("clouds/shapeNoise.csh");
            shapeNoisePipelineConfig = PipelineConfig("clouds/detailNoise.csh");
            shapeNoisePipelineConfig = PipelineConfig("clouds/integrate.csh");
            shapeNoisePipelineConfig = PipelineConfig("clouds/temporal.csh");

		}

		void VolumetricCloudRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene, Graphics::CommandList* commandList) {


			auto clouds = scene->sky.clouds;
			auto sun = scene->sky.sun;
			if (!clouds) return;

			Graphics::Profiler::BeginQuery("Volumetric clouds");

			if (clouds->needsNoiseUpdate) {
				GenerateTextures(scene, commandList);
				clouds->needsNoiseUpdate = false;
			}

			auto downsampledRT = target->GetData(target->GetVolumetricResolution());

			ivec2 res = ivec2(target->volumetricCloudsTexture.width, target->volumetricCloudsTexture.height);

			auto depthTexture = downsampledRT->depthTexture;
			auto velocityTexture = downsampledRT->velocityTexture;
            /*
			{
				Graphics::Profiler::BeginQuery("Integrate");

				ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
				groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
				groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

				integrateShader.Bind();

				target->swapVolumetricCloudsTexture.Bind(GL_WRITE_ONLY, 0);
				depthTexture->Bind(0);
				clouds->shapeTexture.Bind(1);
				clouds->detailTexture.Bind(2);
				blueNoiseTexture.Bind(3);

				integrateShader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
				integrateShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
				integrateShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
				integrateShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);
				integrateShader.GetUniform("cameraLocation")->SetValue(camera->GetLocation());
				
				integrateShader.GetUniform("densityMultiplier")->SetValue(clouds->densityMultiplier);

				integrateShader.GetUniform("planetRadius")->SetValue(scene->sky.planetRadius);
				integrateShader.GetUniform("innerRadius")->SetValue(scene->sky.planetRadius + clouds->minHeight);
				integrateShader.GetUniform("outerRadius")->SetValue(scene->sky.planetRadius + clouds->maxHeight);
				integrateShader.GetUniform("distanceLimit")->SetValue(clouds->distanceLimit);

				integrateShader.GetUniform("lowerHeightFalloff")->SetValue(clouds->lowerHeightFalloff);
				integrateShader.GetUniform("upperHeightFalloff")->SetValue(clouds->upperHeightFalloff);

				integrateShader.GetUniform("shapeScale")->SetValue(clouds->shapeScale);
				integrateShader.GetUniform("detailScale")->SetValue(clouds->detailScale);
				integrateShader.GetUniform("shapeSpeed")->SetValue(clouds->shapeSpeed);
				integrateShader.GetUniform("detailSpeed")->SetValue(clouds->detailSpeed);
				integrateShader.GetUniform("detailStrength")->SetValue(clouds->detailStrength);

				integrateShader.GetUniform("eccentricity")->SetValue(clouds->scattering.eccentricity);
				integrateShader.GetUniform("extinctionFactor")->SetValue(clouds->scattering.extinctionFactor);
				integrateShader.GetUniform("scatteringFactor")->SetValue(clouds->scattering.scatteringFactor);

				integrateShader.GetUniform("silverLiningSpread")->SetValue(clouds->silverLiningSpread);
				integrateShader.GetUniform("silverLiningIntensity")->SetValue(clouds->silverLiningIntensity);

				if (sun) {
					integrateShader.GetUniform("light.direction")->SetValue(sun->direction);
					integrateShader.GetUniform("light.color")->SetValue(sun->color);
					integrateShader.GetUniform("light.intensity")->SetValue(sun->intensity);
				}
				else {
					integrateShader.GetUniform("light.intensity")->SetValue(0.0f);
				}

				integrateShader.GetUniform("time")->SetValue(Clock::Get());
				integrateShader.GetUniform("frameSeed")->SetValue(Common::Random::SampleUniformInt(0, 255));

				commandList->Dispatch(groupCount.x, groupCount.y, 1);

				Graphics::Profiler::EndQuery();
			}

			{
                Graphics::Profiler::BeginQuery("Temporal accumulation");

				ivec2 groupCount = ivec2(res.x / 8, res.y / 8);
				groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
				groupCount.y += ((groupCount.y * 8 == res.y) ? 0 : 1);

				temporalShader.Bind();

				target->swapVolumetricCloudsTexture.Bind(0);
				velocityTexture->Bind(1);
				depthTexture->Bind(2);
				target->historyVolumetricCloudsTexture.Bind(3);
				target->volumetricCloudsTexture.Bind(GL_WRITE_ONLY, 0);

				glMemoryBarrier(GL_ALL_BARRIER_BITS);
				commandList->Dispatch(groupCount.x, groupCount.y, 1);

                Graphics::Profiler::EndQuery();
			}

			glMemoryBarrier(GL_ALL_BARRIER_BITS);
            */
			target->historyVolumetricCloudsTexture = target->volumetricCloudsTexture;
			
			Graphics::Profiler::EndQuery();

		}

		void VolumetricCloudRenderer::GenerateTextures(Scene::Scene* scene, Graphics::CommandList* commandList) {

			auto clouds = scene->sky.clouds;
			if (!clouds) return;

			GenerateShapeTexture(commandList, &clouds->shapeTexture, clouds->shapeScale);
			GenerateDetailTexture(commandList, &clouds->detailTexture, clouds->detailScale);

			clouds->shapeTexture.GenerateMipmap();
			clouds->detailTexture.GenerateMipmap();

		}

		void VolumetricCloudRenderer::GenerateShapeTexture(Graphics::CommandList* commandList,
            Texture::Texture3D* texture, float baseScale) {

			Graphics::Profiler::BeginQuery("Generate shape cloud texture");

			// Expect the resolution to be a power of 2 and larger equal 4
			ivec3 groupCount = ivec3(texture->width, texture->height, texture->depth) / 4;

            auto pipeline = PipelineManager::GetPipeline(shapeNoisePipelineConfig);
            commandList->BindPipeline(pipeline);

            auto randomFloat = Common::Random::SampleUniformFloat() * 10.0f;

            auto constantRange = pipeline->shader->GetPushConstantRange("constants");
            commandList->PushConstants(constantRange, &randomFloat);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(texture->image, 3, 0);
			
			commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

		}

		void VolumetricCloudRenderer::GenerateDetailTexture(Graphics::CommandList* commandList,
            Texture::Texture3D* texture, float baseScale) {

            Graphics::Profiler::BeginQuery("Generate detail cloud texture");

			// Expect the resolution to be a power of 2 and larger equal 4
			ivec3 groupCount = ivec3(texture->width, texture->height, texture->depth) / 4;

            auto pipeline = PipelineManager::GetPipeline(detailNoisePipelineConfig);
            commandList->BindPipeline(pipeline);

            auto randomFloat = Common::Random::SampleUniformFloat() * 10.0f;

            auto constantRange = pipeline->shader->GetPushConstantRange("constants");
            commandList->PushConstants(constantRange, &randomFloat);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_GENERAL, VK_ACCESS_SHADER_WRITE_BIT);

            commandList->BindImage(texture->image, 3, 0);

            commandList->Dispatch(groupCount.x, groupCount.y, groupCount.z);

            commandList->ImageMemoryBarrier(texture->image,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_SHADER_READ_BIT);

            Graphics::Profiler::EndQuery();

		}

	}

}