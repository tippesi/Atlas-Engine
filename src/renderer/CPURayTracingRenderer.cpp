#include "CPURayTracingRenderer.h"

#include "../common/Ray.h"

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

			auto actors = scene->GetMeshActors();

			if (scene->HasChanged())
				PreprocessActors(actors);

			auto corners = camera->GetFrustumCorners(camera->nearPlane, camera->farPlane);

			auto origin = corners[0];
			auto right = corners[1] - origin;
			auto bottom = corners[2] - origin;

			for (int32_t x = 0; x < size.x; x++) {
				for (int32_t y = 0; y < size.y; y++) {

					auto pixel = offset + ivec2(x, y);

					auto coord = vec2((float)x, (float)y) * texelSize;
					
					auto rayDir = glm::normalize(origin + coord.x * right + 
						coord.y * bottom - camera->location);

					Common::Ray ray(camera->location + rayDir * camera->nearPlane, rayDir);

					auto intersect = rayDir * camera->farPlane;
					auto distance = camera->farPlane;
					auto color = vec3(0.0f);

					for (auto& actor : actors) {

						if (!ray.Intersects(actor->aabb, 0.0f, camera->farPlane))
							continue;

						auto indexCount = actor->mesh->data.GetIndexCount();

						auto& vertices = transformedVertices[actor];
						auto indices = actor->mesh->data.indices.Get();

						for (int32_t i = 0; i < indexCount; i += 3) {

							// Retrieve vertices by their index
							auto v0 = vertices[indices[i]];
							auto v1 = vertices[indices[i + 1]];
							auto v2 = vertices[indices[i + 2]];

							// Check for intersection
							vec3 intersection;
							float t;
							if (!ray.Intersects(v0, v1, v2, t, intersection))
								continue;

							if (t >= distance)
								continue;

							distance = t;
							intersect = intersection;
							color = vec3(1.0f, 0.0f, 0.0f);

						}

					}

					uint8_t mono = (uint8_t)((camera->farPlane - distance) / camera->farPlane * 255.0f);

					data[(pixel.y * texture->width + pixel.x) * 4] = mono;
					data[(pixel.y * texture->width + pixel.x) * 4 + 1] = mono;
					data[(pixel.y * texture->width + pixel.x) * 4 + 2] = mono;

				}
			}

			texture->SetData(data);

		}

		void CPURayTracingRenderer::PreprocessActors(std::vector<Actor::MeshActor*> &actors) {

			transformedVertices.clear();

			for (auto& actor : actors) {

				auto vertexCount = actor->mesh->data.GetVertexCount();
				auto vertices = actor->mesh->data.vertices.Get();

				auto mMatrix = actor->transformedMatrix;

				std::vector<vec3> transformed(vertexCount);

				for (int32_t i = 0; i < vertexCount; i++) {
					
					auto vector = vec3(vertices[i * 3],
						vertices[i * 3 + 1], vertices[i * 3 + 2]);

					vector = vec3(mMatrix * vec4(vector, 1.0f));

					transformed[i] = vector;

				}

				transformedVertices[actor] = transformed;

			}

		}

	}

}