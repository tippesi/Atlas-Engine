#include "TerrainRenderer.h"

string TerrainRenderer::vertexPath = "terrain/terrain.vsh";
string TerrainRenderer::tessControlPath = "terrain/terrain.tcsh";
string TerrainRenderer::tessEvalPath = "terrain/terrain.tesh";
string TerrainRenderer::geometryPath = "terrain/terrain.gsh";
string TerrainRenderer::fragmentPath = "terrain/terrain.fsh";

TerrainRenderer::TerrainRenderer() {

	nearShader.AddStage(VERTEX_STAGE, vertexPath);
	nearShader.AddStage(TESSELLATION_CONTROL_STAGE, tessControlPath);
	nearShader.AddStage(TESSELLATION_EVALUATION_STAGE, tessEvalPath);
	// nearShader.AddStage(GEOMETRY_STAGE, geometryPath);
	nearShader.AddStage(FRAGMENT_STAGE, fragmentPath);

	// nearShader.AddMacro("GEOMETRY_SHADER");

	nearShader.Compile();

	GetUniforms();

}

void TerrainRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene) {

	nearShader.Bind();
	heightField->SetValue(0);
	normalMap->SetValue(1);
	diffuseMap->SetValue(2);
	displacementMap->SetValue(3);

	viewMatrix->SetValue(camera->viewMatrix);
	projectionMatrix->SetValue(camera->projectionMatrix);
	cameraLocation->SetValue(camera->location);
	frustumPlanes->SetValue(camera->frustum.planes, 6);

	for (Terrain*& terrain : scene->terrains) {

		modelMatrix->SetValue(mat4(1.0f));

		terrain->Bind();

		heightScale->SetValue(terrain->heightScale);

		tessellationFactor->SetValue(terrain->tessellationFactor);
		tessellationSlope->SetValue(terrain->tessellationSlope);
		tessellationShift->SetValue(terrain->tessellationShift);
		maxTessellationLevel->SetValue(terrain->maxTessellationLevel);

		displacementDistance->SetValue(terrain->displacementDistance);

		for (TerrainNode*& node : terrain->renderList) {

			float patchScale = node->sideLength / 8.0f;

			node->cell->heightField->Bind(GL_TEXTURE0);
			node->cell->normalMap->Bind(GL_TEXTURE1);
			node->cell->diffuseMap->Bind(GL_TEXTURE2);
			node->cell->displacementMap->Bind(GL_TEXTURE3);

			displacementScale->SetValue(.15f);

			nodeLocation->SetValue(node->location);
			nodeSideLength->SetValue(node->sideLength);

			tileScale->SetValue(terrain->resolution * powf(2.0f, (float)(terrain->LoDCount - node->cell->LoD)));
			patchOffsetsScale->SetValue(patchScale);

			glDrawArraysInstanced(GL_PATCHES, 0, terrain->patchVertexCount, 64);

		}

	}

}

void TerrainRenderer::GetUniforms() {

	heightField = nearShader.GetUniform("heightField");
	normalMap = nearShader.GetUniform("normalMap");
	diffuseMap = nearShader.GetUniform("diffuseMap");
	displacementMap = nearShader.GetUniform("displacementMap");
	heightScale = nearShader.GetUniform("heightScale");

	offset = nearShader.GetUniform("offset");
	tileScale = nearShader.GetUniform("tileScale");
	modelMatrix = nearShader.GetUniform("mMatrix");
	viewMatrix = nearShader.GetUniform("vMatrix");
	projectionMatrix = nearShader.GetUniform("pMatrix");
	cameraLocation = nearShader.GetUniform("cameraLocation");
	nodeSideLength = nearShader.GetUniform("nodeSideLength");
	nodeLocation = nearShader.GetUniform("nodeLocation");
	patchOffsetsScale = nearShader.GetUniform("patchOffsetsScale");

	tessellationFactor = nearShader.GetUniform("tessellationFactor");
	tessellationSlope = nearShader.GetUniform("tessellationSlope");
	tessellationShift = nearShader.GetUniform("tessellationShift");
	maxTessellationLevel = nearShader.GetUniform("maxTessellationLevel");

	displacementScale = nearShader.GetUniform("displacementScale");
	displacementDistance = nearShader.GetUniform("displacementDistance");

	frustumPlanes = nearShader.GetUniform("frustumPlanes");

}