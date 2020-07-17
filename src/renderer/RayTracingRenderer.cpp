#include "RayTracingRenderer.h"
#include "../Log.h"
#include "../Clock.h"
#include "../common/Packing.h"

#include "../volume/BVH.h"

#include <unordered_set>
#include <unordered_map>

namespace Atlas {

	namespace Renderer {

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
			primaryRayShader.AddStage(AE_COMPUTE_STAGE, "raytracer/primaryRay.csh");
			primaryRayShader.Compile();

			// Retrieve uniforms
			GetPrimaryRayUniforms();

			bounceUpdateShader.AddStage(AE_COMPUTE_STAGE, "raytracer/bounceUpdate.csh");
			bounceUpdateShader.Compile();

			GetBounceUpdateUniforms();

			rayUpdateShader.AddStage(AE_COMPUTE_STAGE, "raytracer/rayUpdate.csh");
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

			if (baseColorTextureAtlas.slices.size())
				baseColorTextureAtlas.texture.Bind(GL_TEXTURE3);
			if (normalTextureAtlas.slices.size())
				normalTextureAtlas.texture.Bind(GL_TEXTURE4);
			if (roughnessTextureAtlas.slices.size())
				roughnessTextureAtlas.texture.Bind(GL_TEXTURE5);
			if (metalnessTextureAtlas.slices.size())
				metalnessTextureAtlas.texture.Bind(GL_TEXTURE6);
			if (aoTextureAtlas.slices.size())
				aoTextureAtlas.texture.Bind(GL_TEXTURE7);

			if (scene->sky.probe) {
				scene->sky.probe->cubemap.Bind(GL_READ_ONLY, 4);
			}

			materialBuffer.BindBase(5);
			triangleBuffer.BindBase(6);
			nodesBuffer.BindBase(7);			

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

					ivec2 groupCount = resolution / 8 / imageSubdivisions;

					groupCount.x += ((resolution.x % groupCount.x) ? 1 : 0);
					groupCount.y += ((resolution.y % groupCount.y) ? 1 : 0);

					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
						GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

					glDispatchCompute(groupCount.x, groupCount.y, 1);

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

					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
						GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

					glDispatchCompute(1, 1, 1);
				}

				rayUpdateShader.Bind();

				cameraLocationRayUpdateUniform->SetValue(camera->GetLocation());

				if (sun) {
					lightDirectionRayUpdateUniform->SetValue(normalize(sun->direction));
					lightColorRayUpdateUniform->SetValue(sun->color);
					lightIntensityRayUpdateUniform->SetValue(sun->intensity);
				}
				else {
					lightColorRayUpdateUniform->SetValue(vec3(0.0f));
				}

				sampleCountRayUpdateUniform->SetValue(sampleCount);
				bounceCountRayUpdateUniform->SetValue(bounces - i);

				resolutionRayUpdateUniform->SetValue(resolution);
				seedRayUpdateUniform->SetValue(Clock::Get() * (float)(i + 1));

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

				if (!i) {
					uint32_t groupCount = (resolution.x / 4 / imageSubdivisions.x) *
						(resolution.y / 4 / imageSubdivisions.y);
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
			lightIntensityRayUpdateUniform = rayUpdateShader.GetUniform("light.intensity");

			sampleCountRayUpdateUniform = rayUpdateShader.GetUniform("sampleCount");
			bounceCountRayUpdateUniform = rayUpdateShader.GetUniform("bounceCount");

			resolutionRayUpdateUniform = rayUpdateShader.GetUniform("resolution");
			seedRayUpdateUniform = rayUpdateShader.GetUniform("seed");

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

				auto matrix = actor->globalMatrix;

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
					auto handedness = (glm::dot(glm::cross(tangent, normal), t) < 0.0f ? 1.0f : -1.0f);

					auto bitangent = handedness * normalize(glm::cross(tangent, normal));

					// Compress data
					auto pn0 = Common::Packing::PackSignedVector3x10_1x2(vec4(n0, 0.0f));
					auto pn1 = Common::Packing::PackSignedVector3x10_1x2(vec4(n1, 0.0f));
					auto pn2 = Common::Packing::PackSignedVector3x10_1x2(vec4(n2, 0.0f));

					auto pt = Common::Packing::PackSignedVector3x10_1x2(vec4(tangent, 0.0f));
					auto pbt = Common::Packing::PackSignedVector3x10_1x2(vec4(bitangent, 0.0f));

					auto puv0 = glm::packHalf2x16(uv0);
					auto puv1 = glm::packHalf2x16(uv1);
					auto puv2 = glm::packHalf2x16(uv2);

					auto cn0 = reinterpret_cast<float&>(pn0);
					auto cn1 = reinterpret_cast<float&>(pn1);
					auto cn2 = reinterpret_cast<float&>(pn2);

					auto ct = reinterpret_cast<float&>(pt);
					auto cbt = reinterpret_cast<float&>(pbt);

					auto cuv0 = reinterpret_cast<float&>(puv0);
					auto cuv1 = reinterpret_cast<float&>(puv1);
					auto cuv2 = reinterpret_cast<float&>(puv2);

					triangles[k].v0 = vec4(v0, cn0);
					triangles[k].v1 = vec4(v1, cn1);
					triangles[k].v2 = vec4(v2, cn2);
					triangles[k].d0 = vec4(cuv0, cuv1, cuv2, reinterpret_cast<float&>(materialIndices[k]));
					triangles[k].d1 = vec4( ct, cbt, 0.0f, 0.0f);

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
			bvhDepth = bvh.maxDepth;

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

			triangleBuffer.SetSize(triangles.size());
			triangleBuffer.SetData(triangles.data(), 0, triangles.size());

			nodesBuffer.SetSize(gpuNodes.size());
			nodesBuffer.SetData(gpuNodes.data(), 0, gpuNodes.size());

			return true;

		}

		std::unordered_map<Material*, int32_t> RayTracingRenderer::UpdateMaterials(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t materialCount = 0;

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

						gpuMaterial.baseColor = material.baseColor;
						gpuMaterial.emissiveColor = material.emissiveColor;

						gpuMaterial.opacity = material.opacity;

						gpuMaterial.roughness = material.roughness;
						gpuMaterial.metalness = material.metalness;
						gpuMaterial.ao = material.ao;

						gpuMaterial.normalScale = material.normalScale;
						gpuMaterial.invertUVs = actor->mesh->invertUVs ? 1 : 0;

						if (material.HasBaseColorMap()) {
							auto slice = baseColorTextureAtlas.slices[material.baseColorMap];

							gpuMaterial.baseColorTexture.layer = slice.layer;

							gpuMaterial.baseColorTexture.x = slice.offset.x;
							gpuMaterial.baseColorTexture.y = slice.offset.y;

							gpuMaterial.baseColorTexture.width = slice.size.x;
							gpuMaterial.baseColorTexture.height = slice.size.y;
						}
						else {
							gpuMaterial.baseColorTexture.layer = -1;
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

						if (material.HasRoughnessMap()) {
							auto slice = roughnessTextureAtlas.slices[material.roughnessMap];

							gpuMaterial.roughnessTexture.layer = slice.layer;

							gpuMaterial.roughnessTexture.x = slice.offset.x;
							gpuMaterial.roughnessTexture.y = slice.offset.y;

							gpuMaterial.roughnessTexture.width = slice.size.x;
							gpuMaterial.roughnessTexture.height = slice.size.y;
						}
						else {
							gpuMaterial.roughnessTexture.layer = -1;
						}

						if (material.HasMetalnessMap()) {
							auto slice = metalnessTextureAtlas.slices[material.metalnessMap];

							gpuMaterial.metalnessTexture.layer = slice.layer;

							gpuMaterial.metalnessTexture.x = slice.offset.x;
							gpuMaterial.metalnessTexture.y = slice.offset.y;

							gpuMaterial.metalnessTexture.width = slice.size.x;
							gpuMaterial.metalnessTexture.height = slice.size.y;
						}
						else {
							gpuMaterial.metalnessTexture.layer = -1;
						}

						if (material.HasAoMap()) {
							auto slice = aoTextureAtlas.slices[material.aoMap];

							gpuMaterial.aoTexture.layer = slice.layer;

							gpuMaterial.aoTexture.x = slice.offset.x;
							gpuMaterial.aoTexture.y = slice.offset.y;

							gpuMaterial.aoTexture.width = slice.size.x;
							gpuMaterial.aoTexture.height = slice.size.y;
						}
						else {
							gpuMaterial.aoTexture.layer = -1;
						}

						materials.push_back(gpuMaterial);
						materialCount++;
					}
					meshes.insert(actor->mesh);
				}
			}

			materialBuffer.SetSize(materials.size());
			materialBuffer.SetData(materials.data(), 0, materials.size());

			return materialAccess;

		}

		void RayTracingRenderer::UpdateTexture(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			std::unordered_set<Mesh::Mesh*> meshes;
			std::vector<Texture::Texture2D*> baseColorTextures;
			std::vector<Texture::Texture2D*> normalTextures;
			std::vector<Texture::Texture2D*> roughnessTextures;
			std::vector<Texture::Texture2D*> metalnessTextures;
			std::vector<Texture::Texture2D*> aoTextures;

			for (auto& actor : actors) {
				if (meshes.find(actor->mesh) == meshes.end()) {
					auto& actorMaterials = actor->mesh->data.materials;
					for (auto& material : actorMaterials) {
						if (material.HasBaseColorMap())
							baseColorTextures.push_back(material.baseColorMap);
						if (material.HasNormalMap())
							normalTextures.push_back(material.normalMap);
						if (material.HasRoughnessMap())
							roughnessTextures.push_back(material.roughnessMap);
						if (material.HasMetalnessMap())
							metalnessTextures.push_back(material.metalnessMap);
						if (material.HasAoMap())
							aoTextures.push_back(material.aoMap);
					}
					meshes.insert(actor->mesh);
				}
			}

			baseColorTextureAtlas = Texture::TextureAtlas(baseColorTextures);
			normalTextureAtlas = Texture::TextureAtlas(normalTextures);
			roughnessTextureAtlas = Texture::TextureAtlas(roughnessTextures);
			metalnessTextureAtlas = Texture::TextureAtlas(metalnessTextures);
			aoTextureAtlas = Texture::TextureAtlas(aoTextures);

		}

	}

}