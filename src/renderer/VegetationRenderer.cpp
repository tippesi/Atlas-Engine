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

		}

		void VegetationRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, 
			Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap) {

			glDisable(GL_CULL_FACE);

			auto& vegetation = *scene->vegetation;

			auto meshes = vegetation.GetMeshes();

			shader.Bind();

			auto commandBuffer = helper.GetCommandBuffer();
			commandBuffer->Bind();

			shader.GetUniform("vMatrix")->SetValue(camera->viewMatrix);
			shader.GetUniform("pMatrix")->SetValue(camera->projectionMatrix);
			shader.GetUniform("pvMatrixLast")->SetValue(camera->GetLastJitteredMatrix());

			shader.GetUniform("jitterLast")->SetValue(camera->GetLastJitter());
			shader.GetUniform("jitterCurrent")->SetValue(camera->GetJitter());

			shader.GetUniform("time")->SetValue(Clock::Get());
			shader.GetUniform("deltaTime")->SetValue(Clock::GetDelta());

			shader.GetUniform("cameraLocation")->SetValue(camera->GetLocation());

			glMemoryBarrier(GL_COMMAND_BARRIER_BIT);

			// How we order the execution of rendering commands doesn't matter here
			for (auto mesh : meshes) {
				mesh->Bind();
				auto buffers = vegetation.GetBuffers(mesh);

				buffers->culledInstanceData.BindBase(4);

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
						(void*)(sizeof(Helper::VegetationHelper::DrawElementsIndirectCommand) * offset), 1, 0);
				}
			}

			commandBuffer->Unbind();

			glEnable(GL_CULL_FACE);

		}

	}

}