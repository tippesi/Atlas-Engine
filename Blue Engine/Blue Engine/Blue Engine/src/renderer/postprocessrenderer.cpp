#include "postprocessrenderer.h"

PostProcessRenderer::PostProcessRenderer(const char* vertexSource, const char* fragmentSoure) {

	shader = new Shader();

	shader->AddComponent(VERTEX_SHADER, vertexSource);
	shader->AddComponent(FRAGMENT_SHADER, fragmentSoure);

	shader->AddMacro("FILMIC_TONEMAPPING");

	shader->Compile();

	exposure = shader->GetUniform("exposure");
	saturation = shader->GetUniform("saturation");

}

void PostProcessRenderer::Render(RenderTarget* target, Camera* camera, Scene* scene) {



}