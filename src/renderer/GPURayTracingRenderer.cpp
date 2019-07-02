#include "GPURayTracingRenderer.h"

#include "../volume/BVH.h"

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
			nodesBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUBVHNode),
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

			lightDirectionRayCasterUniform->SetValue(normalize(sun->direction));
			lightColorRayCasterUniform->SetValue(sun->color);
			lightAmbientRayCasterUniform->SetValue(sun->ambient);

			texture->Bind(GL_WRITE_ONLY, 0);

			materialBuffer.BindBase(1);
			triangleBuffer.BindBase(2);
			materialIndicesBuffer.BindBase(3);
			nodesBuffer.BindBase(4);

			glDispatchCompute(texture->width / 8, texture->height / 8, 1);

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
						gpuMaterial.diffuseColor = material.diffuseColor;
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

					for (int32_t i = 0; i < count; i++) {
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

			std::vector<Triangle> triangles(triangleCount);

			for (int32_t i = 0; i < triangleCount; i++) {
				triangles[i].v0 = vertices[i * 3];
				triangles[i].v1 = vertices[i * 3 + 1];
				triangles[i].v2 = vertices[i * 3 + 2];
				triangles[i].n0 = normals[i * 3];
				triangles[i].n1 = normals[i * 3 + 1];
				triangles[i].n2 = normals[i * 3 + 2];
				triangles[i].materialIndex = materialIndices[i];
			}

			triangleCount = 0;

			std::vector<Volume::AABB> aabbs;

			for (auto& actor : actors) {

				auto actorTriangleCount = (int32_t)actor->mesh->data.GetIndexCount() / 3;

				auto matrix = actor->transformedMatrix;

				for (int32_t i = 0; i < actorTriangleCount; i++) {

					auto k = i + triangleCount;

					triangles[k].v0 = vec3(matrix * vec4(triangles[k].v0, 1.0f));
					triangles[k].v1 = vec3(matrix * vec4(triangles[k].v1, 1.0f));
					triangles[k].v2 = vec3(matrix * vec4(triangles[k].v2, 1.0f));
					triangles[k].n0 = vec3(matrix * vec4(triangles[k].n0, 0.0f));
					triangles[k].n1 = vec3(matrix * vec4(triangles[k].n1, 0.0f));
					triangles[k].n2 = vec3(matrix * vec4(triangles[k].n2, 0.0f));

					auto min = glm::min(glm::min(triangles[k].v0, triangles[k].v1),
						triangles[k].v2);
					auto max = glm::max(glm::max(triangles[k].v0, triangles[k].v1),
						triangles[k].v2);

					aabbs.push_back(Volume::AABB(min, max));

				}

				triangleCount += actorTriangleCount;

			}

			// Generate BVH
			auto bvh = Volume::BVH<Triangle>(aabbs, triangles);

			// Temporaray data
			triangles = bvh.data;
			aabbs = bvh.aabbs;

			auto nodes = bvh.GetTree();

			// Copy to GPU format
			auto gpuTriangles = std::vector<GPUTriangle>(triangles.size());
			auto gpuNodes = std::vector<GPUBVHNode>(nodes.size());

			// Sorted triangels according to BVH
			for (size_t i = 0; i < triangles.size(); i++) {

				gpuTriangles[i].v0 = vec4(triangles[i].v0, 1.0f);
				gpuTriangles[i].v1 = vec4(triangles[i].v1, 1.0f);
				gpuTriangles[i].v2 = vec4(triangles[i].v2, 1.0f);

				gpuTriangles[i].n0 = vec4(triangles[i].n0, 1.0f);
				gpuTriangles[i].n1 = vec4(triangles[i].n1, 1.0f);
				gpuTriangles[i].n2 = vec4(triangles[i].n2, 1.0f);

				materialIndices[i] = triangles[i].materialIndex;

			}

			// Copy nodes
			for (size_t i = 0; i < nodes.size(); i++) {

				
				if (nodes[i].dataCount) {
					// Leaf node (0x80000000 signalizes a leaf node)
					gpuNodes[i].leaf.dataOffset = nodes[i].dataOffset;
					gpuNodes[i].leaf.dataCount = 0x80000000 | nodes[i].dataCount;
				}
				else {
					// Inner node
					gpuNodes[i].inner.leftChild = nodes[i].leftChild;
					gpuNodes[i].inner.rightChild = nodes[i].rightChild;
				}				

				gpuNodes[i].aabb.min = nodes[i].aabb.min;
				gpuNodes[i].aabb.max = nodes[i].aabb.max;

			}

			materialBuffer.Bind();
			materialBuffer.SetSize(materials.size());
			materialBuffer.SetData(materials.data(), 0, materials.size());

			materialIndicesBuffer.Bind();
			materialIndicesBuffer.SetSize(materialIndices.size());
			materialIndicesBuffer.SetData(materialIndices.data(), 0, materialIndices.size());
			
			triangleBuffer.Bind();
			triangleBuffer.SetSize(gpuTriangles.size());
			triangleBuffer.SetData(gpuTriangles.data(), 0, gpuTriangles.size());

			nodesBuffer.Bind();
			nodesBuffer.SetSize(gpuNodes.size());
			nodesBuffer.SetData(gpuNodes.data(), 0, gpuNodes.size());

			return true;

		}

	}

}