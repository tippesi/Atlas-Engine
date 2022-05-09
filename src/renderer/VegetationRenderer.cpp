#include "VegetationRenderer.h"
#include "../Clock.h"

namespace Atlas {

	namespace Renderer {

		VegetationRenderer::VegetationRenderer() {

			shader.AddStage(AE_VERTEX_STAGE, "vegetation/vegetation.vsh");
			shader.AddStage(AE_FRAGMENT_STAGE, "vegetation/vegetation.fsh");

			shader.AddMacro("BASE_COLOR_MAP");
			shader.AddMacro("OPACITY_MAP");
			shader.Compile();

			depthShader.AddStage(AE_VERTEX_STAGE, "vegetation/depth.vsh");
			depthShader.AddStage(AE_FRAGMENT_STAGE, "vegetation/depth.fsh");

			depthShader.AddMacro("OPACITY_MAP");
			depthShader.Compile();

		}

		void VegetationRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
			Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap) {

			if (!scene->vegetation) return;

			Profiler::BeginQuery("Vegetation");

			glDisable(GL_CULL_FACE);

			auto& vegetation = *scene->vegetation;

			auto meshes = vegetation.GetMeshes();

			auto commandBuffer = helper.GetCommandBuffer();
			commandBuffer->Bind();

			auto time = Clock::Get();
			auto deltaTime = Clock::GetDelta();

			/*
			target->geometryFramebuffer.SetDrawBuffers({});
			
			DepthPrepass(vegetation, meshes, camera, time, deltaTime);

			target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
				GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 });

			glDepthMask(GL_FALSE);
			glDepthFunc(GL_EQUAL);
			*/

			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LEQUAL);
			
			shader.Bind();

			shader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
			shader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
			shader.GetUniform("pvMatrixLast")->SetValue(camera->GetLastJitteredMatrix());

			shader.GetUniform("jitterLast")->SetValue(camera->GetLastJitter());
			shader.GetUniform("jitterCurrent")->SetValue(camera->GetJitter());

			shader.GetUniform("time")->SetValue(time);
			shader.GetUniform("deltaTime")->SetValue(deltaTime);

			glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

			// How we order the execution of rendering commands doesn't matter here
			for (auto mesh : meshes) {
				mesh->Bind();
				auto buffers = vegetation.GetBuffers(mesh);

				buffers->binnedInstanceData.BindBase(5);

				shader.GetUniform("invertUVs")->SetValue(mesh->invertUVs);

				for (auto& subData : mesh->data.subData) {
					auto material = subData.material;

					if (material->HasBaseColorMap())
						material->baseColorMap->Bind(GL_TEXTURE0);
					if (material->HasOpacityMap())
						material->opacityMap->Bind(GL_TEXTURE1);
					if (material->HasNormalMap())
						material->normalMap->Bind(GL_TEXTURE2);
					if (material->HasRoughnessMap())
						material->roughnessMap->Bind(GL_TEXTURE3);
					if (material->HasMetalnessMap())
						material->metalnessMap->Bind(GL_TEXTURE4);
					if (material->HasAoMap())
						material->aoMap->Bind(GL_TEXTURE5);
					if (material->HasDisplacementMap())
						material->displacementMap->Bind(GL_TEXTURE6);

					shader.GetUniform("materialIdx")->SetValue((uint32_t)materialMap[material]);

					auto offset = helper.GetCommandBufferOffset(*mesh, subData);
					glMultiDrawElementsIndirect(mesh->data.primitiveType, mesh->data.indices.GetType(),
						(void*)(sizeof(Helper::VegetationHelper::DrawElementsIndirectCommand) * offset),
						helper.binCount, 0);
				}
			}

			commandBuffer->Unbind();

			glEnable(GL_CULL_FACE);
			glDepthMask(GL_TRUE);
			glDepthFunc(GL_LEQUAL);

			Profiler::EndQuery();

		}

		void VegetationRenderer::DepthPrepass(Scene::Vegetation& vegetation, std::vector<Mesh::VegetationMesh*>& meshes, 
			Camera* camera, float time, float deltaTime) {

			glColorMask(false, false, false, false);

			depthShader.Bind();

			depthShader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
			depthShader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);

			depthShader.GetUniform("time")->SetValue(time);
			depthShader.GetUniform("deltaTime")->SetValue(deltaTime);

			glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

			// How we order the execution of rendering commands doesn't matter here
			for (auto mesh : meshes) {
				mesh->Bind();
				auto buffers = vegetation.GetBuffers(mesh);

				buffers->binnedInstanceData.BindBase(5);

				depthShader.GetUniform("invertUVs")->SetValue(mesh->invertUVs);

				for (auto& subData : mesh->data.subData) {
					auto material = subData.material;

					if (material->HasOpacityMap())
						material->opacityMap->Bind(GL_TEXTURE1);

					auto offset = helper.GetCommandBufferOffset(*mesh, subData);
					glMultiDrawElementsIndirect(mesh->data.primitiveType, mesh->data.indices.GetType(),
						(void*)(sizeof(Helper::VegetationHelper::DrawElementsIndirectCommand) * offset),
						helper.binCount, 0);
				}
			}

			glColorMask(true, true, true, true);

		}

	}

}