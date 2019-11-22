#include "RayTracingRenderer.h"

#include "../volume/BVH.h"
#include "../libraries/glm/packing.hpp"
#include "../libraries/glm/gtc/packing.hpp"

#include <unordered_set>
#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		std::string RayTracingRenderer::vertexUpdateComputePath = "raytracer/vertexUpdate.csh";
		std::string RayTracingRenderer::BVHComputePath = "raytracer/BVHConstruction.csh";
		std::string RayTracingRenderer::rayCasterComputePath = "raytracer/rayCaster.csh";

		RayTracingRenderer::RayTracingRenderer() {

			// Check the possible limits
			glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &shaderStorageLimit);
			glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &textureUnitCount);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupLimit);

			// Create dynamic resizable shader storage buffers
			triangleBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUTriangle),
				AE_BUFFER_DYNAMIC_STORAGE);
			materialBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUMaterial),
				AE_BUFFER_DYNAMIC_STORAGE);
			nodesBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUBVHNode),
				AE_BUFFER_DYNAMIC_STORAGE);

			// Load shader stages from hard drive and compile the shader
			vertexUpdateShader.AddStage(AE_COMPUTE_STAGE, vertexUpdateComputePath);
			vertexUpdateShader.Compile();

			// Retrieve uniforms
			GetVertexUpdateUniforms();

			BVHShader.AddStage(AE_COMPUTE_STAGE, BVHComputePath);
			BVHShader.Compile();

			GetBVHUniforms();

			rayCasterShader.AddStage(AE_COMPUTE_STAGE, rayCasterComputePath);
			rayCasterShader.Compile();

			GetRayCasterUniforms();

		}

		void RayTracingRenderer::Render(Viewport* viewport, RenderTarget* target,
			Camera* camera, Scene::Scene* scene) {



		}

		void RayTracingRenderer::Render(Viewport* viewport, Texture::Texture2D* texture, Texture::Texture2D* inAccumTexture,
			Texture::Texture2D* outAccumTexture, ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene) {

			if (camera->location != cameraLocation || camera->rotation != cameraRotation) {
				
				// Will reset the data
				std::vector<uint8_t> data(texture->width * texture->height * 16, 0);
				inAccumTexture->SetData(data);
				outAccumTexture->SetData(data);
				data.resize(texture->width * texture->height * 4);
				texture->SetData(data);

				cameraLocation = camera->location;
				cameraRotation = camera->rotation;

				sampleCount = 1;
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

			auto cameraLocation = camera->thirdPerson ? camera->location -
				camera->direction * camera->thirdPersonDistance : camera->location;

			// Bind the neccessary shader to do the ray casting and light evaluation
			rayCasterShader.Bind();

			// Set all uniforms which are required by the compute shader
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

			if (sun) {
				lightDirectionRayCasterUniform->SetValue(normalize(sun->direction));
				lightColorRayCasterUniform->SetValue(sun->color);
				lightAmbientRayCasterUniform->SetValue(sun->ambient);
			}
			else {
				lightColorRayCasterUniform->SetValue(vec3(0.0f));
			}

			sampleCountRayCasterUniform->SetValue(sampleCount);
			pixelOffsetRayCasterUniform->SetValue(ivec2(texture->width, texture->height) /
				imageSubdivisions * imageOffset);



			// Bind texture only for writing
			texture->Bind(GL_WRITE_ONLY, 0);
			if (sampleCount % 2 == 0) {
				inAccumTexture->Bind(GL_READ_ONLY, 1);
				outAccumTexture->Bind(GL_WRITE_ONLY, 2);
			}
			else {
				outAccumTexture->Bind(GL_READ_ONLY, 1);
				inAccumTexture->Bind(GL_WRITE_ONLY, 2);
			}
			
			diffuseTextureAtlas.texture.Bind(GL_READ_ONLY, 3);
			normalTextureAtlas.texture.Bind(GL_READ_ONLY, 4);

			// Bind all buffers to their binding points
			materialBuffer.BindBase(1);
			triangleBuffer.BindBase(2);
			nodesBuffer.BindBase(3);

			// Dispatch the compute shader in 
			glDispatchCompute(texture->width / 8 / imageSubdivisions.x,
				texture->height / 8 / imageSubdivisions.y, 1);

			texture->Unbind();

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

		int32_t RayTracingRenderer::GetSampleCount() {

			return sampleCount;

		}

		void RayTracingRenderer::GetVertexUpdateUniforms() {

			modelMatrixVertexUpdateUniform = vertexUpdateShader.GetUniform("mMatrix");
			triangleOffsetVertexUpdateUniform = vertexUpdateShader.GetUniform("triangleOffset");
			triangleCountVertexUpdateUniform = vertexUpdateShader.GetUniform("triangleCount");
			xInvocationsVertexUpdateUniform = vertexUpdateShader.GetUniform("xInvocations");

		}

		void RayTracingRenderer::GetBVHUniforms() {



		}

		void RayTracingRenderer::GetRayCasterUniforms() {

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
			sampleCountRayCasterUniform = rayCasterShader.GetUniform("sampleCount");
			pixelOffsetRayCasterUniform = rayCasterShader.GetUniform("pixelOffset");

		}

		bool RayTracingRenderer::UpdateData(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t indexCount = 0;
			int32_t vertexCount = 0;
			int32_t materialCount = 0;
			uint32_t diffuseMapCount = 0;

			std::unordered_set<Mesh::Mesh*> meshes;
			std::unordered_map<Material*, int32_t> materialAccess;
			std::vector<GPUMaterial> materials;

			UpdateTexture(scene);

			for (auto& actor : actors) {
				indexCount += actor->mesh->data.GetIndexCount();
				vertexCount += actor->mesh->data.GetVertexCount();
				if (meshes.find(actor->mesh) == meshes.end()) {
					auto& actorMaterials = actor->mesh->data.materials;
					for (auto& material : actorMaterials) {
						materialAccess[&material] = materialCount;

						GPUMaterial gpuMaterial;
						
						gpuMaterial.diffuseColor = material.diffuseColor;
						gpuMaterial.emissiveColor = material.emissiveColor;
						gpuMaterial.specularIntensity = material.specularIntensity;
						gpuMaterial.specularHardness = material.specularHardness;
						
						if (material.HasDiffuseMap()) {
							auto slice = diffuseTextureAtlas.slices[material.diffuseMap];

							gpuMaterial.diffuseTexture.layer = slice.layer;

							gpuMaterial.diffuseTexture.x = slice.offset.x;
							gpuMaterial.diffuseTexture.y = slice.offset.x;

							gpuMaterial.diffuseTexture.width = slice.size.x;
							gpuMaterial.diffuseTexture.height = slice.size.x;

						}
						else {
							gpuMaterial.diffuseTexture.layer = -1;
						}
						
						if (material.HasNormalMap()) {
							auto slice = normalTextureAtlas.slices[material.normalMap];

							gpuMaterial.normalTexture.layer = slice.layer;

							gpuMaterial.normalTexture.x = slice.offset.x;
							gpuMaterial.normalTexture.y = slice.offset.x;

							gpuMaterial.normalTexture.width = slice.size.x;
							gpuMaterial.normalTexture.height = slice.size.x;

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

			auto vertexByteCount = vertexCount * sizeof(GPUTriangle);

			if (vertexByteCount > shaderStorageLimit) {
				AtlasLog("Scene has to many data for shader storage buffer objects\n\
					Limit is %d bytes", shaderStorageLimit);
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

			materialBuffer.Bind();
			materialBuffer.SetSize(materials.size());
			materialBuffer.SetData(materials.data(), 0, materials.size());

			triangleBuffer.Bind();
			triangleBuffer.SetSize(triangles.size());
			triangleBuffer.SetData(triangles.data(), 0, triangles.size());

			nodesBuffer.Bind();
			nodesBuffer.SetSize(gpuNodes.size());
			nodesBuffer.SetData(gpuNodes.data(), 0, gpuNodes.size());

			return true;

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