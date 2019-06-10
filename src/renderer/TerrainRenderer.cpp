#include "TerrainRenderer.h"

namespace Atlas {

	namespace Renderer {

		std::string TerrainRenderer::vertexPath = "terrain/terrain.vsh";
		std::string TerrainRenderer::tessControlPath = "terrain/terrain.tcsh";
		std::string TerrainRenderer::tessEvalPath = "terrain/terrain.tesh";
		std::string TerrainRenderer::geometryPath = "terrain/terrain.gsh";
		std::string TerrainRenderer::fragmentPath = "terrain/terrain.fsh";

		TerrainRenderer::TerrainRenderer() {

			nearShader.AddStage(AE_VERTEX_STAGE, vertexPath);
			nearShader.AddStage(AE_TESSELLATION_CONTROL_STAGE, tessControlPath);
			nearShader.AddStage(AE_TESSELLATION_EVALUATION_STAGE, tessEvalPath);
			// nearShader.AddStage(AE_GEOMETRY_STAGE, geometryPath);
			nearShader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			// nearShader.AddMacro("GEOMETRY_SHADER");

			nearShader.Compile();

			GetUniforms();

		}

		void TerrainRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			nearShader.Bind();
			heightField->SetValue(0);
			normalMap->SetValue(1);
			diffuseMap->SetValue(2);
			splatMap->SetValue(3);

			auto mapCount = 3;

			for (int32_t i = 0; i < 4; i++) {
				materials[i].diffuseMap->SetValue(++mapCount);
				materials[i].normalMap->SetValue(++mapCount);
				materials[i].displacementMap->SetValue(++mapCount);
			}

			viewMatrix->SetValue(camera->viewMatrix);
			projectionMatrix->SetValue(camera->projectionMatrix);
			cameraLocation->SetValue(camera->location);
			frustumPlanes->SetValue(camera->frustum.GetPlanes().data(), 6);

			for (auto& terrain : scene->terrains) {

				modelMatrix->SetValue(mat4(1.0f));

				terrain->Bind();

				heightScale->SetValue(terrain->heightScale);

				tessellationFactor->SetValue(terrain->tessellationFactor);
				tessellationSlope->SetValue(terrain->tessellationSlope);
				tessellationShift->SetValue(terrain->tessellationShift);
				maxTessellationLevel->SetValue(terrain->maxTessellationLevel);

				displacementDistance->SetValue(terrain->displacementDistance);

				//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

				for (auto& node : terrain->renderList) {

					float patchScale = node->sideLength / 8.0f;

					node->cell->heightField->Bind(GL_TEXTURE0);
					node->cell->normalMap->Bind(GL_TEXTURE1);
					node->cell->splatMap->Bind(GL_TEXTURE3);

					for (int32_t i = 0; i < 4; i++) {
						auto material = node->cell->GetMaterial(i);
						if (!material)
							continue;
						material->diffuseMap->Bind(GL_TEXTURE0 + i * 3 + 4);
						// material->normalMap->Bind(GL_TEXTURE0 + i * 3 + 5);
						material->displacementMap->Bind(GL_TEXTURE0 + i * 3 + 6);
						materials[i].diffuseColor->SetValue(material->diffuseColor);

						materials[i].specularHardness->SetValue(material->specularHardness);
						materials[i].specularIntensity->SetValue(material->specularIntensity);
					
						materials[i].displacementScale->SetValue(material->displacementScale);
					}

					nodeLocation->SetValue(node->location);
					nodeSideLength->SetValue(node->sideLength);

					leftLoD->SetValue(node->leftLoDStitch);
					topLoD->SetValue(node->topLoDStitch);
					rightLoD->SetValue(node->rightLoDStitch);
					bottomLoD->SetValue(node->bottomLoDStitch);

					tileScale->SetValue(terrain->resolution * powf(2.0f, (float)(terrain->LoDCount - node->cell->LoD)));
					patchSize->SetValue((float)terrain->patchSizeFactor);

					glDrawArraysInstanced(GL_PATCHES, 0, terrain->patchVertexCount, 64);

				}

				// glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			}

		}

		void TerrainRenderer::GetUniforms() {

			heightField = nearShader.GetUniform("heightField");
			normalMap = nearShader.GetUniform("normalMap");
			diffuseMap = nearShader.GetUniform("diffuseMap");
			splatMap = nearShader.GetUniform("splatMap");
			heightScale = nearShader.GetUniform("heightScale");

			offset = nearShader.GetUniform("offset");
			tileScale = nearShader.GetUniform("tileScale");
			modelMatrix = nearShader.GetUniform("mMatrix");
			viewMatrix = nearShader.GetUniform("vMatrix");
			projectionMatrix = nearShader.GetUniform("pMatrix");
			cameraLocation = nearShader.GetUniform("cameraLocation");
			nodeSideLength = nearShader.GetUniform("nodeSideLength");
			nodeLocation = nearShader.GetUniform("nodeLocation");
			patchSize = nearShader.GetUniform("patchSize");

			leftLoD = nearShader.GetUniform("leftLoD");
			topLoD = nearShader.GetUniform("topLoD");
			rightLoD = nearShader.GetUniform("rightLoD");
			bottomLoD = nearShader.GetUniform("bottomLoD");

			tessellationFactor = nearShader.GetUniform("tessellationFactor");
			tessellationSlope = nearShader.GetUniform("tessellationSlope");
			tessellationShift = nearShader.GetUniform("tessellationShift");
			maxTessellationLevel = nearShader.GetUniform("maxTessellationLevel");

			displacementDistance = nearShader.GetUniform("displacementDistance");

			frustumPlanes = nearShader.GetUniform("frustumPlanes");

			for (int32_t i = 0; i < 4; i++) {
				materials[i].diffuseMap = nearShader.GetUniform("materials[" + std::to_string(i) + "].diffuseMap");
				materials[i].normalMap = nearShader.GetUniform("materials[" + std::to_string(i) + "].normalMap");
				materials[i].displacementMap = nearShader.GetUniform("materials[" + std::to_string(i) + "].displacementMap");

				materials[i].diffuseColor = nearShader.GetUniform("materials[" + std::to_string(i) + "].diffuseColor");

				materials[i].specularHardness = nearShader.GetUniform("materials[" + std::to_string(i) + "].specularHardness");
				materials[i].specularIntensity = nearShader.GetUniform("materials[" + std::to_string(i) + "].specularIntensity");

				materials[i].displacementScale = nearShader.GetUniform("materials[" + std::to_string(i) + "].displacementScale");
			}

		}

	}

}