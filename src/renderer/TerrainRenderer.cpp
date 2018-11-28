#include "TerrainRenderer.h"

string TerrainRenderer::vertexPath = "terrain/terrain.vsh";
string TerrainRenderer::tessControlPath = "terrain/terrain.tcsh";
string TerrainRenderer::tessEvalPath = "terrain/terrain.tesh";
string TerrainRenderer::geometryPath = "terrain/terrain.gsh";
string TerrainRenderer::fragmentPath = "terrain/terrain.fsh";

TerrainRenderer::TerrainRenderer() {

	nearShader = new Shader();

	nearShader->AddComponent(VERTEX_SHADER, vertexPath);
	nearShader->AddComponent(TESSELATION_CONTROL_SHADER, tessControlPath);
	nearShader->AddComponent(TESSELATION_EVALUATION_SHADER, tessEvalPath);
	//nearShader->AddComponent(GEOMETRY_SHADER, geometryPath);
	nearShader->AddComponent(FRAGMENT_SHADER, fragmentPath);

	//nearShader->AddMacro("GEOMETRY_SHADER");

	nearShader->Compile();

	GetUniforms();

}

void TerrainRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	nearShader->Bind();
	heightField->SetValue(0);
	normalMap->SetValue(1);

	viewMatrix->SetValue(camera->viewMatrix);
	projectionMatrix->SetValue(camera->projectionMatrix);
	cameraLocation->SetValue(camera->location);

	for (Terrain*& terrain : scene->terrains) {

		modelMatrix->SetValue(mat4(1.0f));

		terrain->Bind();

		heightScale->SetValue(terrain->height);
		patchOffsets->SetValue(terrain->patchOffsets.data(), 64);

		tesselationFactor->SetValue(terrain->tesselationFactor);
		tesselationSlope->SetValue(terrain->tesselationSlope);
		tesselationShift->SetValue(terrain->tesselationShift);

		for (TerrainNode*& node : terrain->renderList) {

			float patchScale = node->sideLength * terrain->resolution / 8.0f;

			node->cell->heightField->Bind(GL_TEXTURE0);
			node->cell->normalMap->Bind(GL_TEXTURE1);

			nodeLocation->SetValue(node->location * patchScale);
			nodeSideLength->SetValue(node->sideLength * patchScale);

			scale->SetValue(patchScale);
			patchOffsetsScale->SetValue(patchScale);

			glDrawArraysInstanced(GL_PATCHES, 0, terrain->patchVertexCount, 64);

		}

	}

}

void TerrainRenderer::GetUniforms() {

	heightField = nearShader->GetUniform("heightField");
	normalMap = nearShader->GetUniform("normalMap");
	heightScale = nearShader->GetUniform("heightScale");
	offset = nearShader->GetUniform("offset");
	scale = nearShader->GetUniform("scale");
	modelMatrix = nearShader->GetUniform("mMatrix");
	viewMatrix = nearShader->GetUniform("vMatrix");
	projectionMatrix = nearShader->GetUniform("pMatrix");
	cameraLocation = nearShader->GetUniform("cameraLocation");
	nodeSideLength = nearShader->GetUniform("nodeSideLength");
	nodeLocation = nearShader->GetUniform("nodeLocation");
	patchOffsets = nearShader->GetUniform("patchOffsets");
	patchOffsetsScale = nearShader->GetUniform("patchOffsetsScale");
	tesselationFactor = nearShader->GetUniform("tesselationFactor");
	tesselationSlope = nearShader->GetUniform("tesselationSlope");
	tesselationShift = nearShader->GetUniform("tesselationShift");

}