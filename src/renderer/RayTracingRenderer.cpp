#include "RayTracingRenderer.h"
#include "../Log.h"

#include "../volume/BVH.h"
#include "../libraries/glm/packing.hpp"
#include "../libraries/glm/gtc/packing.hpp"

#include <unordered_set>
#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		std::string RayTracingRenderer::primaryRayComputePath = "raytracer/primaryRay.csh";
		std::string RayTracingRenderer::bounceUpdateComputePath = "raytracer/bounceUpdate.csh";
		std::string RayTracingRenderer::rayUpdateComputePath = "raytracer/rayUpdate.csh";

		RayTracingRenderer::RayTracingRenderer() {

			// Check the possible limits
			glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &shaderStorageLimit);
			glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureUnitCount);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupLimit);

			indirectDispatchBuffer = Buffer::Buffer(AE_DISPATCH_INDIRECT_BUFFER, 3 *
				sizeof(uint32_t), 0);
			indirectSSBOBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, 3 *
				sizeof(uint32_t), 0);
			counterBuffer0 = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
				sizeof(uint32_t), 0);
			counterBuffer1 = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER,
				sizeof(uint32_t), 0);

			indirectSSBOBuffer.SetSize(1);
			indirectDispatchBuffer.SetSize(1);
			counterBuffer0.SetSize(1);
			counterBuffer1.SetSize(1);

			// Create dynamic resizable shader storage buffers
			triangleBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUTriangle),
				AE_BUFFER_DYNAMIC_STORAGE);
			materialBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUMaterial),
				AE_BUFFER_DYNAMIC_STORAGE);
			nodesBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUBVHNode),
				AE_BUFFER_DYNAMIC_STORAGE);

			// Load shader stages from hard drive and compile the shader
			primaryRayShader.AddStage(AE_COMPUTE_STAGE, primaryRayComputePath);
			primaryRayShader.Compile();

			// Retrieve uniforms
			GetPrimaryRayUniforms();

			bounceUpdateShader.AddStage(AE_COMPUTE_STAGE, bounceUpdateComputePath);
			bounceUpdateShader.Compile();

			GetBounceUpdateUniforms();

			rayUpdateShader.AddStage(AE_COMPUTE_STAGE, rayUpdateComputePath);
			rayUpdateShader.Compile();

			GetRayUpdateUniforms();

		}

		void RayTracingRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {



		}

		void RayTracingRenderer::Render(Viewport* viewport, RayTracerRenderTarget* renderTarget,
			ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene) {

			if (glm::distance(camera->GetLocation(), cameraLocation) > 1e-3f ||
				glm::distance(camera->rotation, cameraRotation) > 1e-3f) {

				cameraLocation = camera->GetLocation();
				cameraRotation = camera->rotation;

				sampleCount = 0;
				imageOffset = ivec2(0);

			}			

			// Check if the scene has changed. A change might happen when an actor has been updated,
			// new actors have been added or old actors have been removed. If this happens we update
			// the data structures.
			if (scene->HasChanged())
				if (!UpdateData(scene)) // Only proceed if GPU data is valid
					return;

			// Retrieve the first directional light source of the scene
			auto lights = scene->GetLights();

			Lighting::DirectionalLight* sun = nullptr;

			for (auto& light : lights) {
				if (light->type == AE_DIRECTIONAL_LIGHT) {
					sun = static_cast<Lighting::DirectionalLight*>(light);
				}
			}

			ivec2 resolution = ivec2(renderTarget->GetWidth(), renderTarget->GetHeight());
			ivec2 tileSize = resolution / imageSubdivisions;

			// Bind texture only for writing
			renderTarget->texture.Bind(GL_WRITE_ONLY, 0);
			if (sampleCount % 2 == 0) {
				renderTarget->accumTexture0.Bind(GL_READ_ONLY, 1);
				renderTarget->accumTexture1.Bind(GL_WRITE_ONLY, 2);
			}
			else {
				renderTarget->accumTexture1.Bind(GL_WRITE_ONLY, 1);
				renderTarget->accumTexture0.Bind(GL_READ_ONLY, 2);
			}

			if (diffuseTextureAtlas.slices.size())
				diffuseTextureAtlas.texture.Bind(GL_READ_ONLY, 3);
			if (normalTextureAtlas.slices.size())
				normalTextureAtlas.texture.Bind(GL_READ_ONLY, 4);

			if (scene->sky.cubemap) {
				scene->sky.cubemap->Bind(GL_READ_ONLY, 5);
			}

			materialBuffer.BindBase(5);
			triangleBuffer.BindBase(6);
			nodesBuffer.BindBase(7);

			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
				GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			for (int32_t i = 0; i <= bounces; i++) {

				if (!i) {
					uint32_t rayCount = tileSize.x * tileSize.y;
					counterBuffer0.Bind();
					counterBuffer0.InvalidateData();
					counterBuffer0.ClearData(AE_R32UI, GL_UNSIGNED_INT, &rayCount);

					uint32_t zero = 0;
					counterBuffer1.Bind();
					counterBuffer1.InvalidateData();
					counterBuffer1.ClearData(AE_R32UI, GL_UNSIGNED_INT, &zero);

					primaryRayShader.Bind();

					auto corners = camera->GetFrustumCorners(camera->nearPlane, camera->farPlane);

					cameraLocationPrimaryRayUniform->SetValue(camera->GetLocation());

					originPrimaryRayUniform->SetValue(corners[4]);
					rightPrimaryRayUniform->SetValue(corners[5] - corners[4]);
					bottomPrimaryRayUniform->SetValue(corners[6] - corners[4]);

					sampleCountPrimaryRayUniform->SetValue(sampleCount);
					pixelOffsetPrimaryRayUniform->SetValue(ivec2(renderTarget->GetWidth(),
						renderTarget->GetHeight()) / imageSubdivisions * imageOffset);

					tileSizePrimaryRayUniform->SetValue(tileSize);
					resolutionPrimaryRayUniform->SetValue(resolution);

					renderTarget->rayBuffer0.BindBase(3);
					renderTarget->rayBuffer1.BindBase(4);

					glDispatchCompute(resolution.x / 8 / imageSubdivisions.x,
						resolution.y / 8 / imageSubdivisions.y, 1);

					counterBuffer0.BindBase(0);
					counterBuffer1.BindBase(1);

					renderTarget->rayBuffer0.BindBase(4);
					renderTarget->rayBuffer1.BindBase(3);
				}
				else {
					if (i % 2 == 0) {
						counterBuffer0.BindBase(0);
						counterBuffer1.BindBase(1);

						renderTarget->rayBuffer0.BindBase(4);
						renderTarget->rayBuffer1.BindBase(3);
					}
					else {
						counterBuffer0.BindBase(1);
						counterBuffer1.BindBase(0);

						renderTarget->rayBuffer0.BindBase(3);
						renderTarget->rayBuffer1.BindBase(4);						
					}

					indirectSSBOBuffer.BindBase(2);

					bounceUpdateShader.Bind();

					glDispatchCompute(1, 1, 1);
				}

				rayUpdateShader.Bind();

				cameraLocationRayUpdateUniform->SetValue(camera->GetLocation());

				if (sun) {
					lightDirectionRayUpdateUniform->SetValue(normalize(sun->direction));
					lightColorRayUpdateUniform->SetValue(sun->color);
					lightAmbientRayUpdateUniform->SetValue(sun->ambient);
				}
				else {
					lightColorRayUpdateUniform->SetValue(vec3(0.0f));
				}

				sampleCountRayUpdateUniform->SetValue(sampleCount);
				bounceCountRayUpdateUniform->SetValue(bounces - i);

				resolutionRayUpdateUniform->SetValue(resolution);

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

				if (!i) {
					uint32_t groupCount = (resolution.x / 8 / imageSubdivisions.x) *
						(resolution.y / 8 / imageSubdivisions.y);
					glDispatchCompute(groupCount, 1, 1);
				}
				else {
					indirectDispatchBuffer.Copy(&indirectSSBOBuffer, 0, 0, 3 * sizeof(uint32_t));
					
					indirectDispatchBuffer.Bind();
					glDispatchComputeIndirect(0);
				}

			}			

			renderTarget->texture.Unbind();

			imageOffset.x++;

			if (imageOffset.x == imageSubdivisions.x) {
				imageOffset.x = 0;
				imageOffset.y++;
			}

			if (imageOffset.y == imageSubdivisions.y) {
				imageOffset.y = 0;
				sampleCount++;
			}

		}

		void RayTracingRenderer::ResetSampleCount() {

			sampleCount = 0;

		}

		int32_t RayTracingRenderer::GetSampleCount() {

			return sampleCount;

		}

		void RayTracingRenderer::GetPrimaryRayUniforms() {

			cameraLocationPrimaryRayUniform = primaryRayShader.GetUniform("cameraLocation");

			originPrimaryRayUniform = primaryRayShader.GetUniform("origin");
			rightPrimaryRayUniform = primaryRayShader.GetUniform("right");
			bottomPrimaryRayUniform = primaryRayShader.GetUniform("bottom");

			sampleCountPrimaryRayUniform = primaryRayShader.GetUniform("sampleCount");
			pixelOffsetPrimaryRayUniform = primaryRayShader.GetUniform("pixelOffset");

			tileSizePrimaryRayUniform = primaryRayShader.GetUniform("tileSize");
			resolutionPrimaryRayUniform = primaryRayShader.GetUniform("resolution");

		}

		void RayTracingRenderer::GetBounceUpdateUniforms() {



		}

		void RayTracingRenderer::GetRayUpdateUniforms() {

			cameraLocationRayUpdateUniform = rayUpdateShader.GetUniform("cameraLocation");
			
			lightDirectionRayUpdateUniform = rayUpdateShader.GetUniform("light.direction");
			lightColorRayUpdateUniform = rayUpdateShader.GetUniform("light.color");
			lightAmbientRayUpdateUniform = rayUpdateShader.GetUniform("light.ambient");

			sampleCountRayUpdateUniform = rayUpdateShader.GetUniform("sampleCount");
			bounceCountRayUpdateUniform = rayUpdateShader.GetUniform("bounceCount");

			resolutionRayUpdateUniform = rayUpdateShader.GetUniform("resolution");

		}

		bool RayTracingRenderer::UpdateData(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t indexCount = 0;
			int32_t vertexCount = 0;

			auto materialAccess = UpdateMaterials(scene);

			for (auto& actor : actors) {
				indexCount += actor->mesh->data.GetIndexCount();
				vertexCount += actor->mesh->data.GetVertexCount();
			}

			auto vertexByteCount = vertexCount * sizeof(GPUTriangle);

			if (vertexByteCount > shaderStorageLimit) {
				Log::Error("Scene has to many data for shader storage buffer objects\n\
					Limit is " + std::to_string(shaderStorageLimit) + " bytes");
				return false;
			}

			int32_t triangleCount = indexCount / 3;

			std::vector<vec3> vertices(indexCount);
			std::vector<vec3> normals(indexCount);
			std::vector<vec2> texCoords(indexCount);
			std::vector<int32_t> materialIndices(triangleCount);

			vertexCount = 0;

			for (auto& actor : actors) {
				auto actorIndexCount = actor->mesh->data.GetIndexCount();
				auto actorVertexCount = actor->mesh->data.GetVertexCount();

				auto actorIndices = actor->mesh->data.indices.Get();
				auto actorVertices = actor->mesh->data.vertices.Get();
				auto actorNormals = actor->mesh->data.normals.Get();
				auto actorTexCoords = actor->mesh->data.texCoords.Get();

				for (auto& subData : actor->mesh->data.subData) {

					auto offset = subData.indicesOffset;
					auto count = subData.indicesCount;
					auto materialIndex = materialAccess[subData.material];

					for (uint32_t i = 0; i < count; i++) {
						auto j = actorIndices[i + offset];
						vertices[vertexCount] = vec3(actorVertices[j * 3], actorVertices[j * 3 + 1],
							actorVertices[j * 3 + 2]);
						normals[vertexCount] = vec3(actorNormals[j * 4], actorNormals[j * 4 + 1],
							actorNormals[j * 4 + 2]);
						if (actor->mesh->data.texCoords.ContainsData())
							texCoords[vertexCount] = vec2(actorTexCoords[j * 2], actorTexCoords[j * 2 + 1]);
						if ((vertexCount % 3) == 0) {
							materialIndices[vertexCount / 3] = materialIndex;
						}
						vertexCount++;
					}
				}

			}

			std::vector<GPUTriangle> triangles(triangleCount);

			triangleCount = 0;

			std::vector<Volume::AABB> aabbs;

			for (auto& actor : actors) {

				auto actorTriangleCount = (int32_t)actor->mesh->data.GetIndexCount() / 3;

				auto matrix = actor->transformedMatrix;

				for (int32_t i = 0; i < actorTriangleCount; i++) {

					auto k = i + triangleCount;

					// Transform everything
					auto v0 = vec3(matrix * vec4(vertices[k * 3], 1.0f));
					auto v1 = vec3(matrix * vec4(vertices[k * 3 + 1], 1.0f));
					auto v2 = vec3(matrix * vec4(vertices[k * 3 + 2], 1.0f));

					auto n0 = normalize(vec3(matrix * vec4(normals[k * 3], 0.0f)));
					auto n1 = normalize(vec3(matrix * vec4(normals[k * 3 + 1], 0.0f)));
					auto n2 = normalize(vec3(matrix * vec4(normals[k * 3 + 2], 0.0f)));

					auto uv0 = texCoords[k * 3];
					auto uv1 = texCoords[k * 3 + 1];
					auto uv2 = texCoords[k * 3 + 2];

					auto v0v1 = v1 - v0;
					auto v0v2 = v2 - v0;

					auto uv0uv1 = uv1 - uv0;
					auto uv0uv2 = uv2 - uv0;

					auto r = 1.0f / (uv0uv1.x * uv0uv2.y - uv0uv2.x * uv0uv1.y);

					auto s = vec3(uv0uv2.y * v0v1.x - uv0uv1.y * v0v2.x,
						uv0uv2.y * v0v1.y - uv0uv1.y * v0v2.y,
						uv0uv2.y * v0v1.z - uv0uv1.y * v0v2.z) * r;

					auto t = vec3(uv0uv1.x * v0v2.x - uv0uv2.x * v0v1.x,
						uv0uv1.x * v0v2.y - uv0uv2.x * v0v1.y,
						uv0uv1.x * v0v2.z - uv0uv2.x * v0v1.z) * r;

					auto normal = normalize(n0 + n1 + n2);

					auto tangent = normalize(s - normal * dot(normal, s));
					auto handedness = glm::dot(glm::cross(tangent, normal), t) < 0.0f ? 1.0f : -1.0f;

					auto bitangent = handedness * normalize(glm::cross(tangent, normal));

					// Compress data
					auto cn0 = PackUnitVector(vec4(n0, 0.0f));
					auto cn1 = PackUnitVector(vec4(n1, 0.0f));
					auto cn2 = PackUnitVector(vec4(n2, 0.0f));

					auto ct = PackUnitVector(vec4(tangent, 0.0f));
					auto cbt = PackUnitVector(vec4(bitangent, 0.0f));

					auto cuv0 = glm::packHalf2x16(uv0);
					auto cuv1 = glm::packHalf2x16(uv1);
					auto cuv2 = glm::packHalf2x16(uv2);

					triangles[k].v0 = vec4(v0, *(float*)& cn0);
					triangles[k].v1 = vec4(v1, *(float*)& cn1);
					triangles[k].v2 = vec4(v2, *(float*)& cn2);
					triangles[k].d0 = vec4(*(float*)& cuv0, *(float*)& cuv1,
						*(float*)& cuv2, *(float*)& materialIndices[k]);
					triangles[k].d1 = vec4(*(float*)& ct, *(float*)& cbt, 0.0f, 0.0f);

					auto min = glm::min(glm::min(triangles[k].v0, triangles[k].v1),
						triangles[k].v2);
					auto max = glm::max(glm::max(triangles[k].v0, triangles[k].v1),
						triangles[k].v2);

					aabbs.push_back(Volume::AABB(min, max));

				}

				triangleCount += actorTriangleCount;

			}

			vertices.clear();
			normals.clear();
			texCoords.clear();

			// Generate BVH
			auto bvh = Volume::BVH<GPUTriangle>(aabbs, triangles);

			// Temporaray data
			triangles = bvh.data;
			aabbs = bvh.aabbs;

			auto nodes = bvh.GetTree();

			// Copy to GPU format
			auto gpuNodes = std::vector<GPUBVHNode>(nodes.size());

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

			triangleBuffer.Bind();
			triangleBuffer.SetSize(triangles.size());
			triangleBuffer.SetData(triangles.data(), 0, triangles.size());

			nodesBuffer.Bind();
			nodesBuffer.SetSize(gpuNodes.size());
			nodesBuffer.SetData(gpuNodes.data(), 0, gpuNodes.size());

			return true;

		}

		std::unordered_map<Material*, int32_t> RayTracingRenderer::UpdateMaterials(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t materialCount = 0;
			uint32_t diffuseMapCount = 0;

			UpdateTexture(scene);

			std::unordered_set<Mesh::Mesh*> meshes;
			std::unordered_map<Material*, int32_t> materialAccess;
			std::vector<GPUMaterial> materials;

			for (auto& actor : actors) {
				if (meshes.find(actor->mesh) == meshes.end()) {
					auto& actorMaterials = actor->mesh->data.materials;
					for (auto& material : actorMaterials) {
						materialAccess[&material] = materialCount;

						GPUMaterial gpuMaterial;

						gpuMaterial.diffuseColor = material.diffuseColor;
						gpuMaterial.emissiveColor = material.emissiveColor;
						gpuMaterial.opacity = material.opacity;
						gpuMaterial.specularIntensity = material.specularIntensity;
						gpuMaterial.specularHardness = material.specularHardness;

						gpuMaterial.normalScale = material.normalScale;
						gpuMaterial.invertUVs = actor->mesh->invertUVs;

						if (material.HasDiffuseMap()) {
							auto slice = diffuseTextureAtlas.slices[material.diffuseMap];

							gpuMaterial.diffuseTexture.layer = slice.layer;

							gpuMaterial.diffuseTexture.x = slice.offset.x;
							gpuMaterial.diffuseTexture.y = slice.offset.y;

							gpuMaterial.diffuseTexture.width = slice.size.x;
							gpuMaterial.diffuseTexture.height = slice.size.y;

						}
						else {
							gpuMaterial.diffuseTexture.layer = -1;
						}

						if (material.HasNormalMap()) {
							auto slice = normalTextureAtlas.slices[material.normalMap];

							gpuMaterial.normalTexture.layer = slice.layer;

							gpuMaterial.normalTexture.x = slice.offset.x;
							gpuMaterial.normalTexture.y = slice.offset.y;

							gpuMaterial.normalTexture.width = slice.size.x;
							gpuMaterial.normalTexture.height = slice.size.y;

						}
						else {
							gpuMaterial.normalTexture.layer = -1;
						}

						materials.push_back(gpuMaterial);
						materialCount++;
					}
					meshes.insert(actor->mesh);
				}
			}

			materialBuffer.Bind();
			materialBuffer.SetSize(materials.size());
			materialBuffer.SetData(materials.data(), 0, materials.size());

			return materialAccess;

		}

		void RayTracingRenderer::UpdateTexture(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			std::unordered_set<Mesh::Mesh*> meshes;
			std::vector<Texture::Texture2D*> diffuseTextures;
			std::vector<Texture::Texture2D*> normalTextures;

			for (auto& actor : actors) {
				if (meshes.find(actor->mesh) == meshes.end()) {
					auto& actorMaterials = actor->mesh->data.materials;
					for (auto& material : actorMaterials) {
						if (material.HasDiffuseMap())
							diffuseTextures.push_back(material.diffuseMap);
						if (material.HasNormalMap())
							normalTextures.push_back(material.normalMap);
					}
					meshes.insert(actor->mesh);
				}
			}

			diffuseTextureAtlas = Texture::TextureAtlas(diffuseTextures);
			normalTextureAtlas = Texture::TextureAtlas(normalTextures);

		}

		int32_t RayTracingRenderer::PackUnitVector(vec4 vector) {

			int32_t packed = 0;

			packed |= (int32_t)((vector.x * 0.5f + 0.5f) * 1023.0f) << 0;
			packed |= (int32_t)((vector.y * 0.5f + 0.5f) * 1023.0f) << 10;
			packed |= (int32_t)((vector.z * 0.5f + 0.5f) * 1023.0f) << 20;
			packed |= (int32_t)((vector.w * 0.5f + 0.5f) * 2.0f) << 30;

			return packed;

		}

	}

}