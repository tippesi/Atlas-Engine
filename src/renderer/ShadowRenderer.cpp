#include "ShadowRenderer.h"

ShaderBatch* ShadowRenderer::shaderBatch;

ShadowRenderer::ShadowRenderer(const char* vertexSource, const char* fragmentSource) {



}


void ShadowRenderer::Render(Window* window, RenderTarget* target, Camera* camera, Scene* scene, bool masterRenderer) {



}

void ShadowRenderer::InitShaderBatch(const char* vertexSource, const char* fragmentSource) {

	shaderBatch = new ShaderBatch();
	shaderBatch->AddComponent(VERTEX_SHADER, vertexSource);
	shaderBatch->AddComponent(FRAGMENT_SHADER, fragmentSource);

}

void ShadowRenderer::AddConfig(ShaderConfig* config) {

	shaderBatch->AddConfig(config);

}

void ShadowRenderer::RemoveConfig(ShaderConfig* config) {

	shaderBatch->RemoveConfig(config);

}