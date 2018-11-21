#include "TerrainRenderer.h"

TerrainRenderer::TerrainRenderer(const char* vertexSource, const char* tessControlSource, const char* tessEvalSource,
	const char* geometrySource, const char* fragmentSource) {

	nearShader = new Shader();

	nearShader->AddComponent(VERTEX_SHADER, vertexSource);
	nearShader->AddComponent(TESSELATION_CONTROL_SHADER, tessControlSource);
	nearShader->AddComponent(TESSELATION_EVALUATION_SHADER, tessEvalSource);
	// nearShader->AddComponent(GEOMETRY_SHADER, geometrySource);
	nearShader->AddComponent(FRAGMENT_SHADER, fragmentSource);

	// nearShader->AddMacro("GEOMETRY_SHADER");

	nearShader->Compile();

	GetUniforms();

}

void TerrainRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {

	nearShader->Bind();
	heightField->SetValue(0);
	normalMap->SetValue(1);

	viewMatrix->SetValue(camera->viewMatrix);
	projectionMatrix->SetValue(camera->projectionMatrix);

	for (Terrain*& terrain : scene->terrains) {

		modelMatrix->SetValue(mat4(1.0f));

		terrain->Bind();

		heightScale->SetValue(terrain->height);

		for (TerrainNode*& node : terrain->renderList) {

			float patchScale = node->sideLength * terrain->resolution / 8.0f;

			node->cell->heightField->Bind(GL_TEXTURE0);
			node->cell->normalMap->Bind(GL_TEXTURE1);

			nodeLocation->SetValue(node->location);
			nodeSideLength->SetValue(node->sideLength);

			scale->SetValue(patchScale / (float)terrain->patchSize);

			// Batch this
			for (int32_t x = 0; x < 8; x++) {
				for (int32_t y = 0; y < 8; y++) {
					
					offset->SetValue(node->location + vec2((float)x, (float)y) * patchScale);
					
					glDrawArrays(GL_PATCHES, 0, terrain->patchVertexCount);
				}
			}

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
	nodeSideLength = nearShader->GetUniform("nodeSideLength");
	nodeLocation = nearShader->GetUniform("nodeLocation");

}