#include "TerrainRenderer.h"

namespace Atlas {

	namespace Renderer {

		std::string TerrainRenderer::vertexPath = "terrain/terrain.vsh";
		std::string TerrainRenderer::tessControlPath = "terrain/terrain.tcsh";
		std::string TerrainRenderer::tessEvalPath = "terrain/terrain.tesh";
		std::string TerrainRenderer::fragmentPath = "terrain/terrain.fsh";

		std::string TerrainRenderer::distanceVertexPath = "terrain/distanceTerrain.vsh";
		std::string TerrainRenderer::distanceFragmentPath = "terrain/distanceTerrain.fsh";

		TerrainRenderer::TerrainRenderer() {

			shader.AddStage(AE_VERTEX_STAGE, vertexPath);
			shader.AddStage(AE_TESSELLATION_CONTROL_STAGE, tessControlPath);
			shader.AddStage(AE_TESSELLATION_EVALUATION_STAGE, tessEvalPath);
			shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			shader.Compile();

			GetUniforms();

			distanceShader.AddStage(AE_VERTEX_STAGE, distanceVertexPath);
			distanceShader.AddStage(AE_FRAGMENT_STAGE, distanceFragmentPath);

			distanceShader.Compile();

			GetDistanceUniforms();

		}

		void TerrainRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			if (!scene->terrain)
				return;

			auto terrain = scene->terrain;

			terrain->UpdateRenderlist(&camera->frustum, camera->GetLocation());

			std::vector<Terrain::TerrainNode*> nodes;
			std::vector<Terrain::TerrainNode*> distanceNodes;
			
			for (auto node : terrain->renderList) {
				if (node->cell->LoD == terrain->LoDCount - 1)
					nodes.push_back(node);
				else
					distanceNodes.push_back(node);
			}

			// First render detail nodes
			shader.Bind();
			terrain->vertexArray.Bind();
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

			heightScale->SetValue(terrain->heightScale);

			tessellationFactor->SetValue(terrain->tessellationFactor);
			tessellationSlope->SetValue(terrain->tessellationSlope);
			tessellationShift->SetValue(terrain->tessellationShift);
			maxTessellationLevel->SetValue(terrain->maxTessellationLevel);

			displacementDistance->SetValue(terrain->displacementDistance);		

			pvMatrixLast->SetValue(pvMatrixPrev);
			jitterLast->SetValue(jitterPrev);
			jitterCurrent->SetValue(camera->GetJitter());

			//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			for (auto node : nodes) {

				node->cell->heightField->Bind(GL_TEXTURE0);
				node->cell->normalMap->Bind(GL_TEXTURE1);
				node->cell->splatMap->Bind(GL_TEXTURE3);

				for (int32_t i = 0; i < 4; i++) {
					auto material = node->cell->GetMaterial(i);
					if (!material)
						continue;
					if (material->diffuseMap)
						material->diffuseMap->Bind(GL_TEXTURE0 + i * 3 + 4);
					if (material->normalMap)
						material->normalMap->Bind(GL_TEXTURE0 + i * 3 + 5);
					if (material->displacementMap)
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

				normalTexelSize->SetValue(1.0f / (float)node->cell->normalMap->width);

				glDrawArraysInstanced(GL_PATCHES, 0, terrain->patchVertexCount, 64);

			}

			// glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			// Now render distance nodes
			distanceShader.Bind();
			terrain->distanceVertexArray.Bind();
			distanceHeightField->SetValue(0);
			distanceNormalMap->SetValue(1);
			distanceDiffuseMap->SetValue(2);
			distanceSplatMap->SetValue(3);

			mapCount = 3;

			for (int32_t i = 0; i < 4; i++) {
				distanceMaterials[i].diffuseMap->SetValue(++mapCount);
			}

			distanceViewMatrix->SetValue(camera->viewMatrix);
			distanceProjectionMatrix->SetValue(camera->projectionMatrix);

			distanceHeightScale->SetValue(terrain->heightScale);

			distancePvMatrixLast->SetValue(pvMatrixPrev);
			distanceJitterLast->SetValue(jitterPrev);
			distanceJitterCurrent->SetValue(camera->GetJitter());

			// glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

			for (auto node : distanceNodes) {

				node->cell->heightField->Bind(GL_TEXTURE0);
				node->cell->normalMap->Bind(GL_TEXTURE1);
				node->cell->splatMap->Bind(GL_TEXTURE3);

				for (int32_t i = 0; i < 4; i++) {
					auto material = node->cell->GetMaterial(i);
					if (!material)
						continue;
					if (material->diffuseMap)
						material->diffuseMap->Bind(GL_TEXTURE0 + i + 4);

					materials[i].diffuseColor->SetValue(material->diffuseColor);

					materials[i].specularHardness->SetValue(material->specularHardness);
					materials[i].specularIntensity->SetValue(material->specularIntensity);
				}

				distanceNodeLocation->SetValue(node->location);
				distanceNodeSideLength->SetValue(node->sideLength);

				distanceLeftLoD->SetValue(node->leftLoDStitch);
				distanceTopLoD->SetValue(node->topLoDStitch);
				distanceRightLoD->SetValue(node->rightLoDStitch);
				distanceBottomLoD->SetValue(node->bottomLoDStitch);

				distanceTileScale->SetValue(terrain->resolution * powf(2.0f, (float)(terrain->LoDCount - node->cell->LoD)));
				distancePatchSize->SetValue((float)terrain->patchSizeFactor);

				distanceNormalTexelSize->SetValue(1.0f / (float)node->cell->normalMap->width);

				glDrawElements(GL_TRIANGLE_STRIP, (int32_t)terrain->distanceVertexArray.GetIndexComponent()->GetElementCount(),
					terrain->distanceVertexArray.GetIndexComponent()->GetDataType(), nullptr);

			}

			// glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

			pvMatrixPrev = camera->unjitterdProjection * camera->viewMatrix;

		}

		void TerrainRenderer::GetUniforms() {

			heightField = shader.GetUniform("heightField");
			normalMap = shader.GetUniform("normalMap");
			diffuseMap = shader.GetUniform("diffuseMap");
			splatMap = shader.GetUniform("splatMap");
			heightScale = shader.GetUniform("heightScale");

			offset = shader.GetUniform("offset");
			tileScale = shader.GetUniform("tileScale");
			viewMatrix = shader.GetUniform("vMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
			cameraLocation = shader.GetUniform("cameraLocation");
			nodeSideLength = shader.GetUniform("nodeSideLength");
			nodeLocation = shader.GetUniform("nodeLocation");
			patchSize = shader.GetUniform("patchSize");

			leftLoD = shader.GetUniform("leftLoD");
			topLoD = shader.GetUniform("topLoD");
			rightLoD = shader.GetUniform("rightLoD");
			bottomLoD = shader.GetUniform("bottomLoD");

			tessellationFactor = shader.GetUniform("tessellationFactor");
			tessellationSlope = shader.GetUniform("tessellationSlope");
			tessellationShift = shader.GetUniform("tessellationShift");
			maxTessellationLevel = shader.GetUniform("maxTessellationLevel");

			displacementDistance = shader.GetUniform("displacementDistance");

			frustumPlanes = shader.GetUniform("frustumPlanes");

			normalTexelSize = shader.GetUniform("normalTexelSize");

			for (int32_t i = 0; i < 4; i++) {
				materials[i].diffuseMap = shader.GetUniform("materials[" + std::to_string(i) + "].diffuseMap");
				materials[i].normalMap = shader.GetUniform("materials[" + std::to_string(i) + "].normalMap");
				materials[i].displacementMap = shader.GetUniform("materials[" + std::to_string(i) + "].displacementMap");

				materials[i].diffuseColor = shader.GetUniform("materials[" + std::to_string(i) + "].diffuseColor");

				materials[i].specularHardness = shader.GetUniform("materials[" + std::to_string(i) + "].specularHardness");
				materials[i].specularIntensity = shader.GetUniform("materials[" + std::to_string(i) + "].specularIntensity");

				materials[i].displacementScale = shader.GetUniform("materials[" + std::to_string(i) + "].displacementScale");
			}

			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");

		}

		void TerrainRenderer::GetDistanceUniforms() {

			distanceHeightField = distanceShader.GetUniform("heightField");
			distanceNormalMap = distanceShader.GetUniform("normalMap");
			distanceDiffuseMap = distanceShader.GetUniform("diffuseMap");
			distanceSplatMap = distanceShader.GetUniform("splatMap");
			distanceHeightScale = distanceShader.GetUniform("heightScale");

			distanceTileScale = distanceShader.GetUniform("tileScale");
			distanceViewMatrix = distanceShader.GetUniform("vMatrix");
			distanceProjectionMatrix = distanceShader.GetUniform("pMatrix");
			distanceNodeSideLength = distanceShader.GetUniform("nodeSideLength");
			distanceNodeLocation = distanceShader.GetUniform("nodeLocation");
			distancePatchSize = distanceShader.GetUniform("patchSize");

			distanceLeftLoD = distanceShader.GetUniform("leftLoD");
			distanceTopLoD = distanceShader.GetUniform("topLoD");
			distanceRightLoD = distanceShader.GetUniform("rightLoD");
			distanceBottomLoD = distanceShader.GetUniform("bottomLoD");

			distanceNormalTexelSize = distanceShader.GetUniform("normalTexelSize");

			for (int32_t i = 0; i < 4; i++) {
				distanceMaterials[i].diffuseMap = distanceShader.GetUniform("materials[" + std::to_string(i) + "].diffuseMap");
				distanceMaterials[i].normalMap = distanceShader.GetUniform("materials[" + std::to_string(i) + "].normalMap");

				distanceMaterials[i].diffuseColor = distanceShader.GetUniform("materials[" + std::to_string(i) + "].diffuseColor");

				distanceMaterials[i].specularHardness = distanceShader.GetUniform("materials[" + std::to_string(i) + "].specularHardness");
				distanceMaterials[i].specularIntensity = distanceShader.GetUniform("materials[" + std::to_string(i) + "].specularIntensity");

				distanceMaterials[i].displacementScale = distanceShader.GetUniform("materials[" + std::to_string(i) + "].displacementScale");
			}

			distancePvMatrixLast = distanceShader.GetUniform("pvMatrixLast");
			distanceJitterLast = distanceShader.GetUniform("jitterLast");
			distanceJitterCurrent = distanceShader.GetUniform("jitterCurrent");

		}

	}

}