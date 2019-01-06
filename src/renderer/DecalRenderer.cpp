#include "DecalRenderer.h"
#include "helper/GeometryHelper.h"

string DecalRenderer::vertexPath = "deferred/decal.vsh";
string DecalRenderer::fragmentPath = "deferred/decal.fsh";

DecalRenderer::DecalRenderer() {

    vertexArray = GeometryHelper::GenerateCubeVertexArray();

    shader.AddStage(VERTEX_STAGE, vertexPath);
    shader.AddStage(FRAGMENT_STAGE, fragmentPath);

	shader.AddMacro("ANIMATION");

    shader.Compile();

    GetUniforms();

}

void DecalRenderer::Render(Window *window, RenderTarget *target, Camera *camera, Scene *scene) {

    uint32_t drawBuffers[] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

    vertexArray->Bind();

    shader.Bind();

    target->geometryFramebuffer->SetDrawBuffers(drawBuffers, 1);

	depthTexture->SetValue(0);
    decalTexture->SetValue(1);

	target->geometryFramebuffer->GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

    viewMatrix->SetValue(camera->viewMatrix);
    projectionMatrix->SetValue(camera->projectionMatrix);
	inverseViewMatrix->SetValue(camera->inverseViewMatrix);
	inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);
	alphaFactor->SetValue(1.0f);

	timeInMilliseconds->SetValue((float)SDL_GetTicks());
	animationLength->SetValue(10000.0f);
	rowCount->SetValue(4.0f);
	columnCount->SetValue(4.0f);

    for (auto& decal : scene->decals) {

        modelMatrix->SetValue(decal->matrix);
        decal->texture->Bind(GL_TEXTURE1);

        glDrawArrays(GL_TRIANGLES, 0, 36);

    }

    target->geometryFramebuffer->SetDrawBuffers(drawBuffers, 3);

}

void DecalRenderer::GetUniforms() {

	depthTexture = shader.GetUniform("depthTexture");
    decalTexture = shader.GetUniform("decalTexture");
    modelMatrix = shader.GetUniform("mMatrix");
    viewMatrix = shader.GetUniform("vMatrix");
    projectionMatrix = shader.GetUniform("pMatrix");
	inverseViewMatrix = shader.GetUniform("ivMatrix");
	inverseProjectionMatrix = shader.GetUniform("ipMatrix");
	alphaFactor = shader.GetUniform("alphaFactor");
	timeInMilliseconds = shader.GetUniform("timeInMilliseconds");
	animationLength = shader.GetUniform("animationLength");
	rowCount = shader.GetUniform("rowCount");
	columnCount = shader.GetUniform("columnCount");

}