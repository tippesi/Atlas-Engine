#include "VolumetricCloudRenderer.h"

#include "common/RandomHelper.h"

namespace Atlas {

	namespace Renderer {

		VolumetricCloudRenderer::VolumetricCloudRenderer() {

			shapeNoiseShader.AddStage(AE_COMPUTE_STAGE, "clouds/shapeNoise.csh");
			shapeNoiseShader.Compile();

			integrateShader.AddStage(AE_COMPUTE_STAGE, "clouds/integrate.csh");
			integrateShader.Compile();

		}

		void VolumetricCloudRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {

			if (!scene->clouds) return;

			Profiler::BeginQuery("Volumetric clouds");

			auto clouds = scene->clouds;

			if (clouds->needsNoiseUpdate) {
				GenerateTextures(scene);
				clouds->needsNoiseUpdate = false;
			}

			auto downsampledRT = target->GetDownsampledTextures(target->GetVolumetricResolution());

			ivec2 res = ivec2(target->volumetricCloudsTexture.width, target->volumetricCloudsTexture.height);

			auto depthTexture = downsampledRT->depthTexture;

			{
				Profiler::BeginQuery("Integrate");

				ivec2 groupCount = ivec2(res.x / 8, res.y / 4);
				groupCount.x += ((groupCount.x * 8 == res.x) ? 0 : 1);
				groupCount.y += ((groupCount.y * 4 == res.y) ? 0 : 1);

				integrateShader.Bind();

				target->volumetricCloudsTexture.Bind(GL_WRITE_ONLY, 0);
				depthTexture->Bind(0);
				clouds->shapeTexture.Bind(1);

				integrateShader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
				integrateShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
				integrateShader.GetUniform("ipMatrix")->SetValue(camera->invProjectionMatrix);
				integrateShader.GetUniform("ivMatrix")->SetValue(camera->invViewMatrix);
				integrateShader.GetUniform("cameraLocation")->SetValue(camera->GetLocation());
				
				integrateShader.GetUniform("shapeScale")->SetValue(clouds->shapeScale);
				integrateShader.GetUniform("densityMultiplier")->SetValue(clouds->densityMultiplier);
				integrateShader.GetUniform("densityCutoff")->SetValue(clouds->densityCutoff);
				integrateShader.GetUniform("aabbMin")->SetValue(clouds->aabbMin);
				integrateShader.GetUniform("aabbMax")->SetValue(clouds->aabbMax);
				
				//integrateShader.GetUniform("sampleCount")->SetValue(camera->invProjectionMatrix);


				glDispatchCompute(groupCount.x, groupCount.y, 1);

				Profiler::EndQuery();
			}
			
			Profiler::EndQuery();

		}

		void VolumetricCloudRenderer::GenerateTextures(Scene::Scene* scene) {

			if (!scene->clouds) return;

			auto clouds = scene->clouds;
			GenerateShapeTexture(&clouds->shapeTexture, clouds->shapeScale);

		}

		void VolumetricCloudRenderer::GenerateShapeTexture(Texture::Texture3D* texture, float baseScale) {

			Profiler::BeginQuery("Generate shape cloud texture");

			// Expect the resolution to be a power of 2 and larger equal 4
			ivec3 groupCount = ivec3(texture->width, texture->height, texture->depth) / 4;

			shapeNoiseShader.Bind();

			texture->Bind(GL_WRITE_ONLY, 0);

			shapeNoiseShader.GetUniform("seed")->SetValue(Common::Random::SampleUniformFloat() * 10.0f);
			
			glDispatchCompute(groupCount.x, groupCount.y, groupCount.z);

			Profiler::EndQuery();

		}

	}

}