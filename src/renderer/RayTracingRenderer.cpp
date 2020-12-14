#include "RayTracingRenderer.h"
#include "../Log.h"
#include "../Clock.h"
#include "../common/Packing.h"

#include "../volume/BVH.h"

#include <unordered_set>
#include <unordered_map>

#define DIRECTIONAL_LIGHT 0
#define TRIANGLE_LIGHT 1

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
			nodeBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPUBVHNode),
				AE_BUFFER_DYNAMIC_STORAGE);
			lightBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(GPULight),
				AE_BUFFER_DYNAMIC_STORAGE, lightCount);

			// Load shader stages from hard drive and compile the shader
			primaryRayShader.AddStage(AE_COMPUTE_STAGE, "raytracer/primaryRay.csh");
			primaryRayShader.Compile();

			// Retrieve uniforms
			GetPrimaryRayUniforms();

			bounceDispatchShader.AddStage(AE_COMPUTE_STAGE, "raytracer/bounceDispatch.csh");
			bounceDispatchShader.Compile();

			bounceUpdateShader.AddStage(AE_COMPUTE_STAGE, "raytracer/bounceUpdate.csh");
			bounceUpdateShader.Compile();

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

			std::vector<GPULight> selectedLights;

			// Randomly select lights (only at image offset 0)
			if (imageOffset.x == 0 && imageOffset.y == 0) {
				if (lights.size() > lightCount) {
					for (size_t i = 0; i < lightBuffer.GetElementCount(); i++) {
						auto rnd = float(rand()) / float(RAND_MAX);

						auto sum = 0.0f;
						for (auto& light : lights) {
							sum += light.data1.y;
							if (rnd < sum) {
								selectedLights.push_back(light);
								break;
							}
						}
					}

					for (auto& light : selectedLights) {
						light.data1.y *= float(selectedLights.size());
					}
				}
				else {
					for (auto light : lights) {
						light.data1.y = 1.0f;
						selectedLights.push_back(light);
					}
				}

				lightBuffer.SetData(selectedLights.data(), 0, selectedLights.size());
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

			if (scene->sky.probe)
				scene->sky.probe->cubemap.Bind(GL_READ_ONLY, 3);

			if (baseColorTextureAtlas.slices.size())
				baseColorTextureAtlas.texture.Bind(GL_TEXTURE4);
			if (opacityTextureAtlas.slices.size())
				opacityTextureAtlas.texture.Bind(GL_TEXTURE5);
			if (normalTextureAtlas.slices.size())
				normalTextureAtlas.texture.Bind(GL_TEXTURE6);
			if (roughnessTextureAtlas.slices.size())
				roughnessTextureAtlas.texture.Bind(GL_TEXTURE7);
			if (metalnessTextureAtlas.slices.size())
				metalnessTextureAtlas.texture.Bind(GL_TEXTURE8);
			if (aoTextureAtlas.slices.size())
				aoTextureAtlas.texture.Bind(GL_TEXTURE9);

			lightBuffer.BindBase(5);
			materialBuffer.BindBase(6);			
			triangleBuffer.BindBase(7);
			nodeBuffer.BindBase(8);			

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

					auto tileResolution = resolution / imageSubdivisions;
					auto groupCount = tileResolution / 8;

					groupCount.x += ((groupCount.x * 8 == tileResolution.x) ? 0 : 1);
					groupCount.y += ((groupCount.y * 8 == tileResolution.y) ? 0 : 1);

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

					bounceDispatchShader.Bind();

					glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
						GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

					glDispatchCompute(1, 1, 1);
				}

				bounceUpdateShader.Bind();

				maxBouncesBounceUpdateUniform->SetValue(bounces);

				sampleCountBounceUpdateUniform->SetValue(sampleCount);
				bounceCountBounceUpdateUniform->SetValue(i);
				lightCountBounceUpdateUniform->SetValue(int32_t(selectedLights.size()));

				resolutionBounceUpdateUniform->SetValue(resolution);
				seedBounceUpdateUniform->SetValue(float(rand()) / float(RAND_MAX));

				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT |
					GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

				if (!i) {
					auto tileResolution = resolution / imageSubdivisions;
					auto tilePixelCount = tileResolution.x * tileResolution.y;
					auto groupCount = tilePixelCount / 32;

					groupCount += ((tilePixelCount % groupCount) ? 1 : 0);
					
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

		int32_t RayTracingRenderer::GetSampleCount() const {

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

		void RayTracingRenderer::GetRayUpdateUniforms() {

			maxBouncesBounceUpdateUniform = bounceUpdateShader.GetUniform("maxBounces");

			sampleCountBounceUpdateUniform = bounceUpdateShader.GetUniform("sampleCount");
			bounceCountBounceUpdateUniform = bounceUpdateShader.GetUniform("bounceCount");
			lightCountBounceUpdateUniform = bounceUpdateShader.GetUniform("lightCount");

			resolutionBounceUpdateUniform = bounceUpdateShader.GetUniform("resolution");
			seedBounceUpdateUniform = bounceUpdateShader.GetUniform("seed");

		}

		bool RayTracingRenderer::UpdateData(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t indexCount = 0;
			int32_t vertexCount = 0;

			std::vector<GPUMaterial> materials;
			auto materialAccess = UpdateMaterials(scene, materials);
			// Finish OpenGL commands instantly to free RAM
			glFinish();

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

			std::vector<Triangle> triangles(triangleCount);

			std::vector<Volume::AABB> aabbs;

			triangleCount = 0;

			for (auto& actor : actors) {

				auto actorTriangleCount = (int32_t)actor->mesh->data.GetIndexCount() / 3;

				auto matrix = actor->globalMatrix;

				for (int32_t i = 0; i < actorTriangleCount; i++) {

					auto k = i + triangleCount;

					// Transform everything
					triangles[k].v0 = vec3(matrix * vec4(vertices[k * 3], 1.0f));
					triangles[k].v1 = vec3(matrix * vec4(vertices[k * 3 + 1], 1.0f));
					triangles[k].v2 = vec3(matrix * vec4(vertices[k * 3 + 2], 1.0f));

					triangles[k].n0 = normalize(vec3(matrix * vec4(normals[k * 3], 0.0f)));
					triangles[k].n1 = normalize(vec3(matrix * vec4(normals[k * 3 + 1], 0.0f)));
					triangles[k].n2 = normalize(vec3(matrix * vec4(normals[k * 3 + 2], 0.0f)));

					triangles[k].uv0 = texCoords[k * 3];
					triangles[k].uv1 = texCoords[k * 3 + 1];
					triangles[k].uv2 = texCoords[k * 3 + 2];

					triangles[k].materialIdx = materialIndices[k];

					auto min = glm::min(glm::min(triangles[k].v0,
						triangles[k].v1), triangles[k].v2);
					auto max = glm::max(glm::max(triangles[k].v0,
						triangles[k].v1), triangles[k].v2);

					aabbs.push_back(Volume::AABB(min, max));

				}

				triangleCount += actorTriangleCount;

			}

			// Force memory to be cleared
			vertices.clear();
			normals.clear();
			texCoords.clear();
			materialIndices.clear();
			vertices.shrink_to_fit();
			normals.shrink_to_fit();
			texCoords.shrink_to_fit();
			materialIndices.shrink_to_fit();

			// Triangle size does not seem to affect
			// BVH traversal to make it worth reducing
			// CutTriangles(aabbs, triangles);

			std::vector<GPUTriangle> gpuTriangles;

			for(auto triangle : triangles) {

				auto v0v1 = triangle.v1 - triangle.v0;
				auto v0v2 = triangle.v2 - triangle.v0;

				auto uv0uv1 = triangle.uv1 - triangle.uv0;
				auto uv0uv2 = triangle.uv2 - triangle.uv0;

				auto r = 1.0f / (uv0uv1.x * uv0uv2.y - uv0uv2.x * uv0uv1.y);

				auto s = vec3(uv0uv2.y * v0v1.x - uv0uv1.y * v0v2.x,
					uv0uv2.y * v0v1.y - uv0uv1.y * v0v2.y,
					uv0uv2.y * v0v1.z - uv0uv1.y * v0v2.z) * r;

				auto t = vec3(uv0uv1.x * v0v2.x - uv0uv2.x * v0v1.x,
					uv0uv1.x * v0v2.y - uv0uv2.x * v0v1.y,
					uv0uv1.x * v0v2.z - uv0uv2.x * v0v1.z) * r;

				auto normal = glm::normalize(triangle.n0 + triangle.n1 + triangle.n2);

				auto tangent = glm::normalize(s - normal * dot(normal, s));
				auto handedness = (glm::dot(glm::cross(tangent, normal), t) < 0.0f ? 1.0f : -1.0f);

				auto bitangent = handedness * glm::normalize(glm::cross(tangent, normal));

				// Compress data
				auto pn0 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n0, 0.0f));
				auto pn1 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n1, 0.0f));
				auto pn2 = Common::Packing::PackSignedVector3x10_1x2(vec4(triangle.n2, 0.0f));

				auto pt = Common::Packing::PackSignedVector3x10_1x2(vec4(tangent, 0.0f));
				auto pbt = Common::Packing::PackSignedVector3x10_1x2(vec4(bitangent, 0.0f));

				auto puv0 = glm::packHalf2x16(triangle.uv0);
				auto puv1 = glm::packHalf2x16(triangle.uv1);
				auto puv2 = glm::packHalf2x16(triangle.uv2);

				auto cn0 = reinterpret_cast<float&>(pn0);
				auto cn1 = reinterpret_cast<float&>(pn1);
				auto cn2 = reinterpret_cast<float&>(pn2);

				auto ct = reinterpret_cast<float&>(pt);
				auto cbt = reinterpret_cast<float&>(pbt);

				auto cuv0 = reinterpret_cast<float&>(puv0);
				auto cuv1 = reinterpret_cast<float&>(puv1);
				auto cuv2 = reinterpret_cast<float&>(puv2);

				GPUTriangle gpuTriangle;

				gpuTriangle.v0 = vec4(triangle.v0, cn0);
				gpuTriangle.v1 = vec4(triangle.v1, cn1);
				gpuTriangle.v2 = vec4(triangle.v2, cn2);
				gpuTriangle.d0 = vec4(cuv0, cuv1, cuv2, reinterpret_cast<float&>(triangle.materialIdx));
				gpuTriangle.d1 = vec4(ct, cbt, 0.0f, 0.0f);

				gpuTriangles.push_back(gpuTriangle);

			}

			triangles.clear();
			triangles.shrink_to_fit();

			// Generate BVH
			auto bvh = Volume::BVH<GPUTriangle>(aabbs, gpuTriangles);

			// Upload triangles
			triangleBuffer.SetSize(bvh.data.size());
			triangleBuffer.SetData(bvh.data.data(), 0, bvh.data.size());

			// Free memory and upload data instantly
			bvh.data.clear();
			bvh.data.shrink_to_fit();
			glFinish();

			auto& nodes = bvh.GetTree();
			auto gpuNodes = std::vector<GPUBVHNode>(nodes.size());
			// Copy to GPU format
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

			// Free original node memory
			nodes.clear();
			nodes.shrink_to_fit();

			// Upload nodes instantly
			nodeBuffer.SetSize(gpuNodes.size());
			nodeBuffer.SetData(gpuNodes.data(), 0, gpuNodes.size());
			glFinish();

			lights.clear();

			// Triangle lights
			for (size_t i = 0; i < gpuTriangles.size(); i++) {
				auto& triangle = gpuTriangles[i];
				auto idx = reinterpret_cast<int32_t&>(triangle.d0.w);
				auto& material = materials[idx];

				auto radiance = material.emissiveColor;
				auto brightness = dot(radiance, vec3(0.3333f));

				if (brightness > 0.0f) {
					// Extract normal information again
					auto cn0 = reinterpret_cast<int32_t&>(triangle.v0.w);
					auto cn1 = reinterpret_cast<int32_t&>(triangle.v1.w);
					auto cn2 = reinterpret_cast<int32_t&>(triangle.v2.w);

					auto n0 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn0));
					auto n1 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn1));
					auto n2 = vec3(Common::Packing::UnpackSignedVector3x10_1x2(cn2));

					// Compute neccessary information
					auto P = (vec3(triangle.v0) + vec3(triangle.v1) + vec3(triangle.v2)) / 3.0f;
					auto N = (n0 + n1 + n2) / 3.0f;

					auto a = glm::distance(vec3(triangle.v1), vec3(triangle.v0));
					auto b = glm::distance(vec3(triangle.v2), vec3(triangle.v0));
					auto c = glm::distance(vec3(triangle.v1), vec3(triangle.v2));
					auto p = 0.5f * (a + b + c);
					auto area = glm::sqrt(p * (p - a) * (p - b) * (p - c));

					auto weight = area * brightness;

					// Compress light
					auto pn = Common::Packing::PackSignedVector3x10_1x2(vec4(N, 0.0f));
					auto cn = reinterpret_cast<float&>(pn);

					uint32_t data = 0;
					data |= (TRIANGLE_LIGHT << 28u);
					data |= uint32_t(i);
					auto cd = reinterpret_cast<float&>(data);

					GPULight light;
					light.data0 = vec4(P, radiance.r);
					light.data1 = vec4(cd, weight, area, radiance.g);
					light.N = vec4(N, radiance.b);

					lights.push_back(light);
				}
			}

			// Find other light sources
			auto lightSources = scene->GetLights();

			for (auto light : lightSources) {
				
				auto radiance = light->color * light->intensity;
				auto brightness = dot(radiance, vec3(0.3333f));

				vec3 P = vec3(0.0f);
				vec3 N = vec3(0.0f);
				float weight = 0.0f;
				float area = 0.0f;

				uint32_t data = 0;

				// Parse individual light information based on type
				if (light->type == AE_DIRECTIONAL_LIGHT) {
					auto dirLight = static_cast<Lighting::DirectionalLight*>(light);
					data |= (DIRECTIONAL_LIGHT << 28u);
					weight = brightness;
					N = dirLight->direction;
				}
				else if (light->type == AE_POINT_LIGHT) {

				}

				data |= uint32_t(lights.size());
				auto cd = reinterpret_cast<float&>(data);

				GPULight gpuLight;
				gpuLight.data0 = vec4(P, radiance.r);
				gpuLight.data1 = vec4(cd, weight, area, radiance.g);
				gpuLight.N = vec4(N, radiance.b);

				lights.push_back(gpuLight);				

			}

			// Find the maximum weight
			auto maxWeight = 0.0f;
			for (auto& light : lights) {
				maxWeight = glm::max(maxWeight, light.data1.y);
			}

			// Calculate min weight and adjust lights based on it
			auto minWeight = 0.02f * maxWeight;
			// Also calculate the total weight
			auto totalWeight = 0.0f;

			for (auto& light : lights) {
				light.data1.y = glm::max(light.data1.y, minWeight);
				totalWeight += light.data1.y;
			}

			for (auto& light : lights) {
				light.data1.y /= totalWeight;
			}

			return true;

		}

		void RayTracingRenderer::UpdateMaterials(Scene::Scene* scene) {

			std::vector<GPUMaterial> materials;
			UpdateMaterials(scene, materials);

		}

		std::unordered_map<Material*, int32_t> RayTracingRenderer::UpdateMaterials(Scene::Scene* scene,
			std::vector<GPUMaterial>& materials) {

			auto actors = scene->GetMeshActors();

			int32_t materialCount = 0;

			UpdateTexture(scene);

			std::unordered_set<Mesh::Mesh*> meshes;
			std::unordered_map<Material*, int32_t> materialAccess;

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

						if (material.HasOpacityMap()) {
							auto slice = opacityTextureAtlas.slices[material.opacityMap];

							gpuMaterial.opacityTexture.layer = slice.layer;

							gpuMaterial.opacityTexture.x = slice.offset.x;
							gpuMaterial.opacityTexture.y = slice.offset.y;

							gpuMaterial.opacityTexture.width = slice.size.x;
							gpuMaterial.opacityTexture.height = slice.size.y;
						}
						else {
							gpuMaterial.opacityTexture.layer = -1;
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
			std::vector<Texture::Texture2D*> opacityTextures;
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
						if (material.HasOpacityMap())
							opacityTextures.push_back(material.opacityMap);
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
			opacityTextureAtlas = Texture::TextureAtlas(opacityTextures);
			normalTextureAtlas = Texture::TextureAtlas(normalTextures);
			roughnessTextureAtlas = Texture::TextureAtlas(roughnessTextures);
			metalnessTextureAtlas = Texture::TextureAtlas(metalnessTextures);
			aoTextureAtlas = Texture::TextureAtlas(aoTextures);

		}

		void RayTracingRenderer::CutTriangles(std::vector<Volume::AABB>& aabbs, std::vector<Triangle>& triangles) {

			const float threshold = 1.0f / 16.0f;

			auto min = vec3(std::numeric_limits<float>::max());
			auto max = vec3(-std::numeric_limits<float>::max());

			for (auto& aabb : aabbs) {
				min = glm::min(aabb.min, min);
				max = glm::max(aabb.max, max);
			}

			auto diff = max - min;

			bool foundAll = false;
			size_t idx = 0;
			std::vector<Triangle> cutTriangles;

			// Find triangles which are too large
			while (!foundAll) {
				for (int32_t j = 0; j < 3; j++) {
					auto size = aabbs[idx].max[j] - aabbs[idx].min[j];
					// Compare each axis to a relative threshold
					if (size > threshold * diff[j]) {
						cutTriangles.push_back(triangles[idx]);
						// Remove from global array
						triangles[idx] = triangles[triangles.size() - 1];
						aabbs[idx] = aabbs[aabbs.size() - 1];
						triangles.pop_back();
						aabbs.pop_back();
						// Need to check the moved item
						idx--;
						// Exit inner loop
						break;
					}
				}
				foundAll = ++idx == triangles.size();
			}

			// Find triangles with no area

			while (cutTriangles.size()) {
				foundAll = false;
				idx = 0;
				// Find triangles which are too small to be cutted
				while (!foundAll) {
					auto& tri = cutTriangles[idx];
					Volume::AABB aabb(glm::min(tri.v0, glm::min(tri.v1, tri.v2)),
						glm::max(tri.v0, glm::max(tri.v1, tri.v2)));
					uint32_t smallAxisCount = 0;
					for (int32_t j = 0; j < 3; j++) {
						auto size = aabb.max[j] - aabb.min[j];
						// Compare each axis to a relative threshold
						if (size <= threshold * diff[j]) {
							smallAxisCount++;
						}
					}
					// All three axis have to be below the threshold
					if (smallAxisCount == 3) {
						// Add to global array
						triangles.push_back(tri);
						aabbs.push_back(aabb);
						// Remove from cut
						cutTriangles[idx] = cutTriangles[cutTriangles.size() - 1];
						cutTriangles.pop_back();
						// Need to check the moved item
						idx--;
					}
					foundAll = ++idx == cutTriangles.size();
				}

				std::vector<Triangle> cuttedTriangles;

				// Cut triangles
				for (size_t i = 0; i < cutTriangles.size(); i++) {
					auto& tri = cutTriangles[i];
					Volume::AABB aabb(glm::min(tri.v0, glm::min(tri.v1, tri.v2)),
						glm::max(tri.v0, glm::max(tri.v1, tri.v2)));
					auto maxExtend = 0.0f;
					int32_t maxAxis = 0;
					// Find largest axiss
					for (int32_t j = 0; j < 3; j++) {
						auto size = aabb.max[j] - aabb.min[j];
						if (size > maxExtend) {
							maxExtend = size;
							maxAxis = j;
						}
					}
					// Cut in half
					maxExtend /= 2.0f;
					auto cuttingPoint = 0.5f * (aabb.min[maxAxis] + aabb.max[maxAxis]);
					// Find vertices on negative and positive side
					// of cutting point. Need to find the side with
					// only one vertex
					int32_t negativeSide = 0;
					negativeSide += tri.v0[maxAxis] < cuttingPoint ? 1 : 0;
					negativeSide += tri.v1[maxAxis] < cuttingPoint ? 1 : 0;
					negativeSide += tri.v2[maxAxis] < cuttingPoint ? 1 : 0;

					// Find index of the single vertex
					int32_t singleIdx = 0;
					if (negativeSide < 2) {
						singleIdx = tri.v1[maxAxis] < cuttingPoint ? 1 : singleIdx;
						singleIdx = tri.v2[maxAxis] < cuttingPoint ? 2 : singleIdx;
					}
					else {
						singleIdx = tri.v1[maxAxis] >= cuttingPoint ? 1 : singleIdx;
						singleIdx = tri.v2[maxAxis] >= cuttingPoint ? 2 : singleIdx;
					}

					// Construct a triangle with new order in mind
					Triangle t;
					switch (singleIdx) {
					case 0: t.v0 = tri.v0; t.v1 = tri.v1; t.v2 = tri.v2;
						t.n0 = tri.n0; t.n1 = tri.n1; t.n2 = tri.n2;
						t.uv0 = tri.uv0; t.uv1 = tri.uv1; t.uv2 = tri.uv2; break;
					case 1: t.v0 = tri.v1; t.v1 = tri.v0; t.v2 = tri.v2;
						t.n0 = tri.n1; t.n1 = tri.n0; t.n2 = tri.n2;
						t.uv0 = tri.uv1; t.uv1 = tri.uv0; t.uv2 = tri.uv2; break;
					case 2: t.v0 = tri.v2; t.v1 = tri.v1; t.v2 = tri.v0;
						t.n0 = tri.n2; t.n1 = tri.n1; t.n2 = tri.n0;
						t.uv0 = tri.uv2; t.uv1 = tri.uv1; t.uv2 = tri.uv0; break;
					}
					t.materialIdx = tri.materialIdx;

					// Calculate edges and distance t on edge to cutting point
					auto e0 = t.v1 - t.v0;
					auto e1 = t.v2 - t.v0;

					auto t0 = (cuttingPoint - t.v0[maxAxis]) / e0[maxAxis];
					auto t1 = (cuttingPoint - t.v0[maxAxis]) / e1[maxAxis];

					// With v0, v1, v2, p0, p1 we can now create three triangles
					// which have their max/mins at the cutting points
					cuttedTriangles.push_back(t.Subdivide(vec2(0.0f, 0.0f), vec2(t0, 0.0f), vec2(0.0f, t1)));
					cuttedTriangles.push_back(t.Subdivide(vec2(t0, 0.0f), vec2(1.0, 0.0f), vec2(0.0f, 1.0f)));
					cuttedTriangles.push_back(t.Subdivide(vec2(0.0f, 1.0f), vec2(t0, 0.0f), vec2(0.0f, t1)));
				}

				cutTriangles = cuttedTriangles;

			}
			
		}

	}

}