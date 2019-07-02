#include "CPURayTracingRenderer.h"

#include "../volume/Ray.h"

#include <vector>

namespace Atlas {

	namespace Renderer {


		CPURayTracingRenderer::CPURayTracingRenderer() {



		}

		void CPURayTracingRenderer::Render(Viewport* window, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			

		}

		void CPURayTracingRenderer::Render(Viewport* viewport, Texture::Texture2D* texture, Camera* camera, Scene::Scene* scene) {

			auto data = texture->GetData();

			auto offset = ivec2(viewport->x, viewport->y);
			auto size = ivec2(viewport->width, viewport->height);

			auto texelSize = 1.0f / vec2((float)viewport->width, (float)viewport->height);

			auto lights = scene->GetLights();

			Lighting::DirectionalLight* sun = nullptr;

			for (auto& light : lights) {
				if (light->type == AE_DIRECTIONAL_LIGHT) {
					sun = static_cast<Lighting::DirectionalLight*>(light);
				}
			}

			if (!sun)
				return;

			if (scene->HasChanged())
				UpdateData(scene);

			auto cameraLocation = camera->thirdPerson ? camera->location -
				camera->direction * camera->thirdPersonDistance : camera->location;

			auto corners = camera->GetFrustumCorners(camera->nearPlane, camera->farPlane);

			auto origin = corners[0];
			auto right = corners[1] - origin;
			auto bottom = corners[2] - origin;

			for (int32_t x = 0; x < size.x; x++) {
				for (int32_t y = 0; y < size.y; y++) {

					auto pixel = offset + ivec2(x, y);

					auto coord = vec2((float)x, (float)y) * texelSize;
					
					auto rayDir = glm::normalize(origin + coord.x * right + 
						coord.y * bottom - cameraLocation);

					Volume::Ray ray(cameraLocation + rayDir * camera->nearPlane, rayDir);

					auto barrycentric = vec2(0.0f);
					auto index = 0;
					auto distance = camera->farPlane;

					int32_t count = 0;

					for (auto& triangle : triangles) {
						count++;

						// Retrieve vertices by their index
						auto& v0 = triangle.v0;
						auto& v1 = triangle.v1;
						auto& v2 = triangle.v2;

						// Check for intersection
						vec3 intersection;
						if (!ray.Intersects(v0, v1, v2, intersection))
							continue;

						if (intersection.x >= distance)
							continue;

						distance = intersection.x;
						barrycentric = vec2(intersection.y,
							intersection.z);
						index = count - 1;

					}

					vec3 color(0.0f);

					if (distance < camera->farPlane)
						color = EvaluateLight(sun, index, barrycentric);

					data[(pixel.y * texture->width + pixel.x) * 4] = (uint8_t)(glm::clamp(color.r, 0.0f, 1.0f) * 255.0f);
					data[(pixel.y * texture->width + pixel.x) * 4 + 1] = (uint8_t)(glm::clamp(color.g, 0.0f, 1.0f) * 255.0f);
					data[(pixel.y * texture->width + pixel.x) * 4 + 2] = (uint8_t)(glm::clamp(color.b, 0.0f, 1.0f) * 255.0f);

				}
			}

			texture->SetData(data);

		}

		vec3 CPURayTracingRenderer::EvaluateLight(Lighting::DirectionalLight* light, int32_t index, vec2 barrycentric) {

			const float gamma = 1.0f / 2.2f;

			auto& triangle = triangles[index];

			auto n0 = triangle.n0;
			auto n1 = triangle.n1;
			auto n2 = triangle.n2;

			auto normal = (1.0f - barrycentric.x - barrycentric.y) * n0 +
				barrycentric.x * n1 + barrycentric.y * n2;

			auto surfaceColor = materials[triangle.materialIndex]->diffuseColor;

			vec3 specular(0.0f);
			vec3 diffuse(1.0f);
			vec3 ambient(light->ambient * surfaceColor);

			diffuse = glm::max(glm::dot(normal, -light->direction) * light->color, 0.0f) * surfaceColor;

			auto color = diffuse + specular + ambient;

			return glm::pow(color, vec3(gamma));

		}

		void CPURayTracingRenderer::UpdateData(Scene::Scene* scene) {

			auto actors = scene->GetMeshActors();

			int32_t indexCount = 0;
			int32_t vertexCount = 0;
			int32_t materialCount = 0;

			std::unordered_set<Mesh::Mesh*> meshes;
			std::unordered_map<Material*, int32_t> materialAccess;

			for (auto& actor : actors) {
				indexCount += actor->mesh->data.GetIndexCount();
				vertexCount += actor->mesh->data.GetVertexCount();
				if (meshes.find(actor->mesh) == meshes.end()) {
					auto& actorMaterials = actor->mesh->data.materials;
					for (auto& material : actorMaterials) {
						materialAccess[&material] = materialCount;
						materials.push_back(&material);
						materialCount++;
					}
				}
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

			triangles.resize(triangleCount);

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

				}

				triangleCount += actorTriangleCount;

			}

		}

	}

}