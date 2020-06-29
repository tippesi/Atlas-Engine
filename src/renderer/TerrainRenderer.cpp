#include "TerrainRenderer.h"

namespace Atlas {

	namespace Renderer {

		TerrainRenderer::TerrainRenderer() {

			shader.AddStage(AE_VERTEX_STAGE, "terrain/terrain.vsh");
			shader.AddStage(AE_TESSELLATION_CONTROL_STAGE, "terrain/terrain.tcsh");
			shader.AddStage(AE_TESSELLATION_EVALUATION_STAGE, "terrain/terrain.tesh");
			shader.AddStage(AE_FRAGMENT_STAGE, "terrain/terrain.fsh");

			shader.Compile();

			GetUniforms();

			distanceShader.AddStage(AE_VERTEX_STAGE, "terrain/terrain.vsh");
			distanceShader.AddStage(AE_TESSELLATION_CONTROL_STAGE, "terrain/terrain.tcsh");
			distanceShader.AddStage(AE_TESSELLATION_EVALUATION_STAGE, "terrain/terrain.tesh");
			distanceShader.AddStage(AE_FRAGMENT_STAGE, "terrain/terrain.fsh");

			distanceShader.AddMacro("DISTANCE");

			distanceShader.Compile();

			GetDistanceUniforms();

			terrainMaterialBuffer = Buffer::Buffer(AE_UNIFORM_BUFFER, sizeof(TerrainMaterial),
				AE_BUFFER_DYNAMIC_STORAGE);
			terrainMaterialBuffer.SetSize(256);

		}

		void TerrainRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
			Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap) {

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

			auto materials = terrain->storage->GetMaterials();

			std::vector<TerrainMaterial> terrainMaterials(materials.size());

			for (size_t i = 0; i < materials.size(); i++) {
				if (materials[i]) {
					terrainMaterials[i].idx = (uint32_t)materialMap[materials[i]];
					terrainMaterials[i].roughness = materials[i]->roughness;
					terrainMaterials[i].metalness = materials[i]->metalness;
					terrainMaterials[i].ao = materials[i]->ao;
					terrainMaterials[i].displacementScale = materials[i]->displacementScale;
					terrainMaterials[i].normalScale = materials[i]->normalScale;
				}

			}

			terrainMaterialBuffer.SetData(terrainMaterials.data(), 0, terrainMaterials.size());

			// First render detail nodes
			shader.Bind();
			terrainMaterialBuffer.BindBase(0);
			terrain->vertexArray.Bind();

			terrain->storage->baseColorMaps.Bind(GL_TEXTURE3);
			terrain->storage->roughnessMaps.Bind(GL_TEXTURE4);
			terrain->storage->aoMaps.Bind(GL_TEXTURE5);
			terrain->storage->normalMaps.Bind(GL_TEXTURE6);
			terrain->storage->displacementMaps.Bind(GL_TEXTURE7);

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

			pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
			jitterLast->SetValue(camera->GetLastJitter());
			jitterCurrent->SetValue(camera->GetJitter());

#ifdef AE_API_GL
			if (terrain->wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

			for (auto node : nodes) {

				node->cell->heightField->Bind(GL_TEXTURE0);
				node->cell->normalMap->Bind(GL_TEXTURE1);
				node->cell->splatMap->Bind(GL_TEXTURE2);

				nodeLocation->SetValue(node->location);
				nodeSideLength->SetValue(node->sideLength);

				leftLoD->SetValue(node->leftLoDStitch);
				topLoD->SetValue(node->topLoDStitch);
				rightLoD->SetValue(node->rightLoDStitch);
				bottomLoD->SetValue(node->bottomLoDStitch);

				tileScale->SetValue(terrain->resolution * powf(2.0f, (float)(terrain->LoDCount - node->cell->LoD) - 1.0f));
				patchSize->SetValue((float)terrain->patchSizeFactor);

				normalTexelSize->SetValue(1.0f / (float)node->cell->normalMap->width);

				glDrawArraysInstanced(GL_PATCHES, 0, terrain->patchVertexCount, 64);

			}

			// Now render distance nodes
			distanceShader.Bind();

			distanceViewMatrix->SetValue(camera->viewMatrix);
			distanceProjectionMatrix->SetValue(camera->projectionMatrix);
			distanceCameraLocation->SetValue(camera->location);
			distanceFrustumPlanes->SetValue(camera->frustum.GetPlanes().data(), 6);

			distanceHeightScale->SetValue(terrain->heightScale);

			distancePvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
			distanceJitterLast->SetValue(camera->GetLastJitter());
			distanceJitterCurrent->SetValue(camera->GetJitter());

			for (auto node : distanceNodes) {

				node->cell->heightField->Bind(GL_TEXTURE0);
				node->cell->normalMap->Bind(GL_TEXTURE1);
				node->cell->splatMap->Bind(GL_TEXTURE2);

				distanceNodeLocation->SetValue(node->location);
				distanceNodeSideLength->SetValue(node->sideLength);

				distanceLeftLoD->SetValue(node->leftLoDStitch);
				distanceTopLoD->SetValue(node->topLoDStitch);
				distanceRightLoD->SetValue(node->rightLoDStitch);
				distanceBottomLoD->SetValue(node->bottomLoDStitch);

				distanceTileScale->SetValue(terrain->resolution * powf(2.0f, (float)(terrain->LoDCount - node->cell->LoD) - 1.0f));
				distancePatchSize->SetValue((float)terrain->patchSizeFactor);

				distanceNormalTexelSize->SetValue(1.0f / (float)node->cell->normalMap->width);

				glDrawArraysInstanced(GL_PATCHES, 0, terrain->patchVertexCount, 64);

			}

#ifdef AE_API_GL
			if (terrain->wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

		}

		void TerrainRenderer::GetUniforms() {

			heightScale = shader.GetUniform("heightScale");

			offset = shader.GetUniform("offset");
			tileScale = shader.GetUniform("tileScale");
			viewMatrix = shader.GetUniform("vMatrix");
			projectionMatrix = shader.GetUniform("pMatrix");
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

			cameraLocation = shader.GetUniform("cameraLocation");
			frustumPlanes = shader.GetUniform("frustumPlanes");

			normalTexelSize = shader.GetUniform("normalTexelSize");

			pvMatrixLast = shader.GetUniform("pvMatrixLast");
			jitterLast = shader.GetUniform("jitterLast");
			jitterCurrent = shader.GetUniform("jitterCurrent");

		}

		void TerrainRenderer::GetDistanceUniforms() {

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

			distanceCameraLocation = distanceShader.GetUniform("cameraLocation");
			distanceFrustumPlanes = distanceShader.GetUniform("frustumPlanes");

			distanceNormalTexelSize = distanceShader.GetUniform("normalTexelSize");

			distancePvMatrixLast = distanceShader.GetUniform("pvMatrixLast");
			distanceJitterLast = distanceShader.GetUniform("jitterLast");
			distanceJitterCurrent = distanceShader.GetUniform("jitterCurrent");

		}

	}

}