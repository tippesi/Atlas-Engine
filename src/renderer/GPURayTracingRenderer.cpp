#include "GPURayTracingRenderer.h"

namespace Atlas {

	namespace Renderer {

		std::string GPURayTracingRenderer::vertexUpdateComputePath = "raytracer/vertexUpdate.csh";
		std::string GPURayTracingRenderer::BVHComputePath = "raytracer/BVHConstruction.csh";
		std::string GPURayTracingRenderer::rayCasterComputePath = "raytracer/rayCaster.csh";

		GPURayTracingRenderer::GPURayTracingRenderer() {

			glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &shaderStorageLimit);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupLimit);

			triangleBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUTriangle), AE_BUFFER_DYNAMIC_STORAGE);

			vertexUpdateShader.AddStage(AE_COMPUTE_STAGE, vertexUpdateComputePath);
			vertexUpdateShader.Compile();

			GetVertexUpdateUniforms();

			BVHShader.AddStage(AE_COMPUTE_STAGE, BVHComputePath);
			BVHShader.Compile();

			GetBVHUniforms();

			rayCasterShader.AddStage(AE_COMPUTE_STAGE, rayCasterComputePath);
			rayCasterShader.Compile();

			GetRayCasterUniforms();

		}

		void GPURayTracingRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {



		}

		void GPURayTracingRenderer::Render(Viewport* viewport, Texture::Texture2D* texture,
			Camera* camera, Scene::Scene* scene) {

			if (scene->HasChanged())
				if (!UpdateGPUData(scene))
					return;

			rayCasterShader.Bind();

			widthRayCasterUniform->SetValue(texture->width);
			heightRayCasterUniform->SetValue(texture->height);

			auto corners = camera->GetFrustumCorners(camera->nearPlane, camera->farPlane);

			originRayCasterUniform->SetValue(corners[0]);
			rightRayCasterUniform->SetValue(corners[1] - corners[0]);
			bottomRayCasterUniform->SetValue(corners[2] - corners[0]);

			cameraLocationRayCasterUniform->SetValue(camera->location);
			cameraFarPlaneRayCasterUniform->SetValue(camera->farPlane);
			cameraNearPlaneRayCasterUniform->SetValue(camera->nearPlane);

			triangleCountRayCasterUniform->SetValue((int32_t)triangleBuffer.GetElementCount());

			texture->Bind(GL_WRITE_ONLY, 0);

			triangleBuffer.BindBase(1);

			glDispatchCompute(texture->width / 16, texture->height / 16, 1);

		}

		void GPURayTracingRenderer::GetVertexUpdateUniforms() {

			modelMatrixVertexUpdateUniform = vertexUpdateShader.GetUniform("mMatrix");
			triangleOffsetVertexUpdateUniform = vertexUpdateShader.GetUniform("triangleOffset");
			triangleCountVertexUpdateUniform = vertexUpdateShader.GetUniform("triangleCount");
			xInvocationsVertexUpdateUniform = vertexUpdateShader.GetUniform("xInvocations");

		}

		void GPURayTracingRenderer::GetBVHUniforms() {



		}

		void GPURayTracingRenderer::GetRayCasterUniforms() {

			widthRayCasterUniform = rayCasterShader.GetUniform("width");
			heightRayCasterUniform = rayCasterShader.GetUniform("height");
			originRayCasterUniform = rayCasterShader.GetUniform("origin");
			rightRayCasterUniform = rayCasterShader.GetUniform("right");
			bottomRayCasterUniform = rayCasterShader.GetUniform("bottom");
			cameraLocationRayCasterUniform = rayCasterShader.GetUniform("cameraLocation");
			cameraFarPlaneRayCasterUniform = rayCasterShader.GetUniform("cameraFarPlane");
			cameraNearPlaneRayCasterUniform = rayCasterShader.GetUniform("cameraNearPlane");
			triangleCountRayCasterUniform = rayCasterShader.GetUniform("triangleCount");

		}

		bool GPURayTracingRenderer::UpdateGPUData(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t indexCount = 0;
			int32_t vertexCount = 0;

			for (auto& actor : actors) {
				indexCount += actor->mesh->data->GetIndexCount();
				vertexCount += actor->mesh->data->GetVertexCount();
			}

			auto vertexByteCount = vertexCount * sizeof(vec4);

			if (vertexByteCount > shaderStorageLimit) {
				AtlasLog("Scene has to many data for shader storage buffer objects\n\
					Limit is %d bytes", shaderStorageLimit);
				return false;
			}

			std::vector<vec4> vertices(indexCount);

			vertexCount = 0;

			for (auto& actor : actors) {
				auto actorIndexCount = actor->mesh->data->GetIndexCount();
				auto actorVertexCount = actor->mesh->data->GetVertexCount();

				auto actorIndices = actor->mesh->data->indices->Get();
				auto actorVertices = actor->mesh->data->vertices->Get();

				for (int32_t i = 0; i < actorIndexCount; i++) {
					auto j = actorIndices[i];
					vertices[vertexCount++] = vec4(actorVertices[j * 3], actorVertices[j * 3 + 1],
							actorVertices[j * 3 + 2], 1.0f);
				}

			}

			int32_t triangleCount = indexCount / 3;
			std::vector<GPUTriangle> triangles(triangleCount);

			for (int32_t i = 0; i < triangleCount; i++) {
				triangles[i].v0 = vertices[i * 3];
				triangles[i].v1 = vertices[i * 3 + 1];
				triangles[i].v2 = vertices[i * 3 + 2];
			}

			triangleBuffer.SetSize(triangles.size());
			triangleBuffer.SetData(triangles.data(), 0, triangles.size());

			
			vertexUpdateShader.Bind();

			triangleBuffer.BindBase(1);

			triangleCount = 0;
			int32_t xInvocations = 0, yInvocations = 0;

			for (auto& actor : actors) {
				
				auto actorTriangleCount = (int32_t)actor->mesh->data->GetIndexCount() / 3;

				triangleOffsetVertexUpdateUniform->SetValue(triangleCount);
				triangleCountVertexUpdateUniform->SetValue(actorTriangleCount);

				// Check if our work group count is to large
				// Some vendors like Intel just support the minimum of 65536.
				if (actorTriangleCount < workGroupLimit) {
					xInvocations = actorTriangleCount;
					yInvocations = 1;
				}
				else {
					auto computeDimension = (int32_t)ceilf(sqrtf((float)actorTriangleCount));
					xInvocations = computeDimension;
					yInvocations = computeDimension;
				}

				xInvocationsVertexUpdateUniform->SetValue(xInvocations);
				modelMatrixVertexUpdateUniform->SetValue(actor->transformedMatrix);

				glDispatchCompute(xInvocations, yInvocations, 1);

				triangleCount += actorTriangleCount;

			}

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			return true;

		}

	}

}