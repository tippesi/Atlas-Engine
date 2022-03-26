#include "TerrainRenderer.h"

namespace Atlas {

	namespace Renderer {

		TerrainRenderer::TerrainRenderer() {

			shaderBatch.AddStage(AE_VERTEX_STAGE, "terrain/terrain.vsh");
			shaderBatch.AddStage(AE_TESSELLATION_CONTROL_STAGE, "terrain/terrain.tcsh");
			shaderBatch.AddStage(AE_TESSELLATION_EVALUATION_STAGE, "terrain/terrain.tesh");
			shaderBatch.AddStage(AE_FRAGMENT_STAGE, "terrain/terrain.fsh");

			distanceConfig.AddMacro("DISTANCE");

			shaderBatch.AddConfig(&detailConfig);
			shaderBatch.AddConfig(&distanceConfig);

			GetUniforms();

			terrainMaterialBuffer = Buffer::Buffer(AE_UNIFORM_BUFFER, sizeof(TerrainMaterial),
				AE_BUFFER_DYNAMIC_STORAGE);
			terrainMaterialBuffer.SetSize(256);

		}

		void TerrainRenderer::Render(Viewport* viewport, RenderTarget* target, Camera* camera,
			Scene::Scene* scene, std::unordered_map<void*, uint16_t> materialMap) {

			if (!scene->terrain)
				return;

			Profiler::BeginQuery("Terrain");

			auto terrain = scene->terrain;

			terrain->UpdateRenderlist(&camera->frustum, camera->GetLocation());

			std::vector<Terrain::TerrainNode*> detailNodes;
			std::vector<Terrain::TerrainNode*> distanceNodes;

			for (auto node : terrain->renderList) {
				if (node->cell->LoD >= terrain->LoDCount - terrain->detailNodeIdx)
					detailNodes.push_back(node);
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

			terrainMaterialBuffer.BindBase(0);
			terrain->vertexArray.Bind();

			terrain->storage->baseColorMaps.Bind(GL_TEXTURE3);
			terrain->storage->roughnessMaps.Bind(GL_TEXTURE4);
			terrain->storage->aoMaps.Bind(GL_TEXTURE5);
			terrain->storage->normalMaps.Bind(GL_TEXTURE6);
			terrain->storage->displacementMaps.Bind(GL_TEXTURE7);

#ifdef AE_API_GL
			if (terrain->wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
#endif

			for (uint8_t i = 0; i < 2; i++) {

				std::vector<Terrain::TerrainNode*> nodes;

				switch (i) {
				case 0: shaderBatch.Bind(&detailConfig);
					Profiler::BeginQuery("Detail");
					nodes = detailNodes;
					break;
				case 1: shaderBatch.Bind(&distanceConfig);
					Profiler::BeginQuery("Distance");
					nodes = distanceNodes;
					break;
				default: break;
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

				pvMatrixLast->SetValue(camera->GetLastJitteredMatrix());
				jitterLast->SetValue(camera->GetLastJitter());
				jitterCurrent->SetValue(camera->GetJitter());

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
					patchSize->SetValue(float(terrain->patchSizeFactor));

					normalTexelSize->SetValue(1.0f / float(node->cell->normalMap->width));

					glDrawArraysInstanced(GL_PATCHES, 0, terrain->patchVertexCount, 64);

				}

				Profiler::EndQuery();

			}

#ifdef AE_API_GL
			if (terrain->wireframe)
				glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
#endif

			Profiler::EndQuery();

		}

		void TerrainRenderer::GetUniforms() {

			heightScale = shaderBatch.GetUniform("heightScale");

			offset = shaderBatch.GetUniform("offset");
			tileScale = shaderBatch.GetUniform("tileScale");
			viewMatrix = shaderBatch.GetUniform("vMatrix");
			projectionMatrix = shaderBatch.GetUniform("pMatrix");
			nodeSideLength = shaderBatch.GetUniform("nodeSideLength");
			nodeLocation = shaderBatch.GetUniform("nodeLocation");
			patchSize = shaderBatch.GetUniform("patchSize");

			leftLoD = shaderBatch.GetUniform("leftLoD");
			topLoD = shaderBatch.GetUniform("topLoD");
			rightLoD = shaderBatch.GetUniform("rightLoD");
			bottomLoD = shaderBatch.GetUniform("bottomLoD");

			tessellationFactor = shaderBatch.GetUniform("tessellationFactor");
			tessellationSlope = shaderBatch.GetUniform("tessellationSlope");
			tessellationShift = shaderBatch.GetUniform("tessellationShift");
			maxTessellationLevel = shaderBatch.GetUniform("maxTessellationLevel");

			displacementDistance = shaderBatch.GetUniform("displacementDistance");

			cameraLocation = shaderBatch.GetUniform("cameraLocation");
			frustumPlanes = shaderBatch.GetUniform("frustumPlanes");

			normalTexelSize = shaderBatch.GetUniform("normalTexelSize");

			pvMatrixLast = shaderBatch.GetUniform("pvMatrixLast");
			jitterLast = shaderBatch.GetUniform("jitterLast");
			jitterCurrent = shaderBatch.GetUniform("jitterCurrent");

		}

	}

}