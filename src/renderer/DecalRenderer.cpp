#include "DecalRenderer.h"
#include "helper/GeometryHelper.h"

#include "../Clock.h"

namespace Atlas {

    namespace Renderer {

        std::string DecalRenderer::vertexPath = "deferred/decal.vsh";
        std::string DecalRenderer::fragmentPath = "deferred/decal.fsh";

        DecalRenderer::DecalRenderer() {

            Helper::GeometryHelper::GenerateCubeVertexArray(vertexArray);

            shader.AddStage(AE_VERTEX_STAGE, vertexPath);
            shader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

            shader.AddMacro("ANIMATION");

            shader.Compile();

            GetUniforms();

        }

        void DecalRenderer::Render(Viewport *viewport, RenderTarget *target, Camera *camera, Scene::Scene *scene) {

            vertexArray.Bind();

            shader.Bind();

			target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0 });

            target->geometryFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(GL_TEXTURE0);

            viewMatrix->SetValue(camera->viewMatrix);
            projectionMatrix->SetValue(camera->projectionMatrix);
            inverseViewMatrix->SetValue(camera->inverseViewMatrix);
            inverseProjectionMatrix->SetValue(camera->inverseProjectionMatrix);

            timeInMilliseconds->SetValue(1000.0f * Clock::Get());

			Volume::AABB base(vec3(-1.0f), vec3(1.0f));
			auto aabb = base.Transform(glm::inverse(camera->projectionMatrix * camera->viewMatrix));

			auto decalActors = scene->GetDecalActors(aabb);

            for (auto& decalActor : decalActors) {

				auto decal = decalActor->decal;

                rowCount->SetValue(decal->rowCount);
                columnCount->SetValue(decal->columnCount);
                animationLength->SetValue(decal->animationLength);

                modelMatrix->SetValue(decalActor->transformedMatrix);

                color->SetValue(decalActor->color);

                decal->texture->Bind(GL_TEXTURE1);

                glDrawArrays(GL_TRIANGLES, 0, 36);

            }

            target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });

        }

        void DecalRenderer::GetUniforms() {

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

    }

}