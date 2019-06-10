#include "GPURayTracingRenderer.h"

#include <unordered_set>
#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		std::string GPURayTracingRenderer::vertexUpdateComputePath = "raytracer/vertexUpdate.csh";
		std::string GPURayTracingRenderer::BVHComputePath = "raytracer/BVHConstruction.csh";
		std::string GPURayTracingRenderer::rayCasterComputePath = "raytracer/rayCaster.csh";

		GPURayTracingRenderer::GPURayTracingRenderer() {

			glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &shaderStorageLimit);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupLimit);

			triangleBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUTriangle),
				AE_BUFFER_DYNAMIC_STORAGE);
			materialBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUMaterial),
				AE_BUFFER_DYNAMIC_STORAGE);
			materialIndicesBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(int32_t),
				AE_BUFFER_DYNAMIC_STORAGE);

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

			auto lights = scene->GetLights();

			Lighting::DirectionalLight* sun = nullptr;

			for (auto& light : lights) {
				if (light->type == AE_DIRECTIONAL_LIGHT) {
					sun = static_cast<Lighting::DirectionalLight*>(light);
				}
			}

			if (!sun)
				return;

			auto cameraLocation = camera->thirdPerson ? camera->location -
				camera->direction * camera->thirdPersonDistance : camera->location;

			rayCasterShader.Bind();

			widthRayCasterUniform->SetValue(texture->width);
			heightRayCasterUniform->SetValue(texture->height);

			auto corners = camera->GetFrustumCorners(camera->nearPlane, camera->farPlane);

			originRayCasterUniform->SetValue(corners[0]);
			rightRayCasterUniform->SetValue(corners[1] - corners[0]);
			bottomRayCasterUniform->SetValue(corners[2] - corners[0]);

			cameraLocationRayCasterUniform->SetValue(cameraLocation);
			cameraFarPlaneRayCasterUniform->SetValue(camera->farPlane);
			cameraNearPlaneRayCasterUniform->SetValue(camera->nearPlane);

			triangleCountRayCasterUniform->SetValue((int32_t)triangleBuffer.GetElementCount());

			lightDirectionRayCasterUniform->SetValue(sun->direction);
			lightColorRayCasterUniform->SetValue(sun->color);
			lightAmbientRayCasterUniform->SetValue(sun->ambient);

			texture->Bind(GL_WRITE_ONLY, 0);

			triangleBuffer.BindBase(1);
			materialIndicesBuffer.BindBase(2);
			materialBuffer.BindBase(3);

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
			lightDirectionRayCasterUniform = rayCasterShader.GetUniform("light.direction");
			lightColorRayCasterUniform = rayCasterShader.GetUniform("light.color");
			lightAmbientRayCasterUniform = rayCasterShader.GetUniform("light.ambient");

		}

		bool GPURayTracingRenderer::UpdateGPUData(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t indexCount = 0;
			int32_t vertexCount = 0;
			int32_t materialCount = 0;

			std::unordered_set<Mesh::Mesh*> meshes;
			std::unordered_map<Material*, int32_t> materialAccess;
			std::vector<GPUMaterial> materials;

			for (auto& actor : actors) {
				indexCount += actor->mesh->data.GetIndexCount();
				vertexCount += actor->mesh->data.GetVertexCount();
				if (meshes.find(actor->mesh) == meshes.end()) {
					auto& actorMaterials = actor->mesh->data.materials;
					for (auto& material : actorMaterials) {
						materialAccess[&material] = materialCount;
						GPUMaterial gpuMaterial;
						gpuMaterial.diffuseColor = vec4(material.diffuseColor, 1.0f);
						gpuMaterial.specularIntensity = material.specularIntensity;
						gpuMaterial.specularHardness = material.specularHardness;
						materials.push_back(gpuMaterial);
						materialCount++;
					}
				}
			}

			auto vertexByteCount = vertexCount * sizeof(GPUTriangle);

			if (vertexByteCount > shaderStorageLimit) {
				AtlasLog("Scene has to many data for shader storage buffer objects\n\
					Limit is %d bytes", shaderStorageLimit);
				return false;
			}

			int32_t triangleCount = indexCount / 3;

			std::vector<vec4> vertices(indexCount);
			std::vector<vec4> normals(indexCount);
			std::vector<int32_t> materialIndices(triangleCount);

			vertexCount = 0;

			for (auto& actor : actors) {
				auto actorIndexCount = actor->mesh->data.GetIndexCount();
				auto actorVertexCount = actor->mesh->data.GetVertexCount();

				auto actorIndices = actor->mesh->data.indices.Get();
				auto actorVertices = actor->mesh->data.vertices.Get();
				auto actorNormals = actor->mesh->data.normals.Get();

				for (auto& subData : actor->mesh->data.subData) {

					auto offset = subData.indicesOffset;
					auto count = subData.indicesCount;
					auto materialIndex = materialAccess[subData.material];

					for (int32_t i = 0; i < actorIndexCount; i++) {
						auto j = actorIndices[i + offset];
						vertices[vertexCount] = vec4(actorVertices[j * 3], actorVertices[j * 3 + 1],
							actorVertices[j * 3 + 2], 1.0f);
						normals[vertexCount] = vec4(actorNormals[j * 4], actorNormals[j * 4 + 1],
							actorNormals[j * 4 + 2], 0.0f);
						if ((vertexCount % 3) == 0) {
							materialIndices[vertexCount / 3] = materialIndex;
						}
						vertexCount++;
					}
				}

			}

			std::vector<GPUTriangle> triangles(triangleCount);

			for (int32_t i = 0; i < triangleCount; i++) {
				triangles[i].v0 = vertices[i * 3];
				triangles[i].v1 = vertices[i * 3 + 1];
				triangles[i].v2 = vertices[i * 3 + 2];
				triangles[i].n0 = normals[i * 3];
				triangles[i].n1 = normals[i * 3 + 1];
				triangles[i].n2 = normals[i * 3 + 2];
			}

			triangleBuffer.SetSize(triangles.size());
			triangleBuffer.SetData(triangles.data(), 0, triangles.size());

			materialBuffer.SetSize(materials.size());
			materialBuffer.SetData(materials.data(), 0, materials.size());

			materialIndicesBuffer.SetSize(materialIndices.size());
			materialIndicesBuffer.SetData(materialIndices.data(), 0, materialIndices.size());
			
			vertexUpdateShader.Bind();

			triangleBuffer.BindBase(1);

			triangleCount = 0;
			int32_t xInvocations = 0, yInvocations = 0;

			for (auto& actor : actors) {
				
				auto actorTriangleCount = (int32_t)actor->mesh->data.GetIndexCount() / 3;

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