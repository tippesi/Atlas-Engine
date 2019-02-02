#include "DecalRenderer.h"
#include "helper/GeometryHelper.h"

std::string DecalRenderer::vertexPath = "deferred/decal.vsh";
std::string DecalRenderer::fragmentPath = "deferred/decal.fsh";

DecalRenderer::DecalRenderer() {

    GeometryHelper::GenerateCubeVertexArray(vertexArray);

    shader.AddStage(AE_VERTEX_STAGE, vertexPath);
    shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

	shader.AddMacro("ANIMATION");

    shader.Compile();

    GetUniforms();

}

void DecalRenderer::Render(Window *window, RenderTarget *target, Camera *camera, Scene *scene) {

    std::vector<uint32_t> drawBuffers = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

    vertexArray.Bind();

    shader.Bind();

    target->geometryFramebuffer->SetDrawBuffers(drawBuffers);

	depthTexture->SetValue(0);
    decalTexture->SetValue(1);

	target->geometryFramebuffer->GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

    viewMatrix->SetValue(camera->viewMatrix);
    projectionMatrix->SetValue(camera->projectionMatrix);
	inverseViewMatrix->SetValue(camera->inverseViewMatrix);
	inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

	timeInMilliseconds->SetValue((float)SDL_GetTicks());

    for (auto& decal : scene->decals) {

        rowCount->SetValue(decal->rowCount);
        columnCount->SetValue(decal->columnCount);
        animationLength->SetValue(decal->animationLength);

        modelMatrix->SetValue(decal->matrix);

		color->SetValue(decal->color);

        decal->texture->Bind(GL_TEXTURE1);

        glDrawArrays(GL_TRIANGLES, 0, 36);

    }

    target->geometryFramebuffer->SetDrawBuffers(drawBuffers);

}

void DecalRenderer::GetUniforms() {

	depthTexture = shader.GetUniform("depthTexture");
    decalTexture = shader.GetUniform("decalTexture");
    modelMatrix = shader.GetUniform("mMatrix");
    viewMatrix = shader.GetUniform("vMatrix");
    projectionMatrix = shader.GetUniform("pMatrix");
	inverseViewMatrix = shader.GetUniform("ivMatrix");
	inverseProjectionMatrix = shader.GetUniform("ipMatrix");
	timeInMilliseconds = shader.GetUniform("timeInMilliseconds");
	animationLength = shader.GetUniform("animationLength");
	rowCount = shader.GetUniform("rowCount");
	columnCount = shader.GetUniform("columnCount");
	color = shader.GetUniform("color");

}