#include "ImpostorTool.h"
#include "../RenderTarget.h"
#include "../renderer/OpaqueRenderer.h"
#include "../scene/Scene.h"

namespace Atlas {

	namespace Tools {

		Mesh::Impostor* ImpostorTool::GenerateImpostor(Mesh::Mesh* mesh, 
			int32_t views, int32_t resolution, bool octahedron) {

			const bool orthoProjection = false;

			Renderer::OpaqueRenderer renderer;
			Viewport viewport(0, 0, resolution, resolution);

			//auto framebuffer = new Framebuffer(resolution, resolution);
            /*
			auto depthTexture = new Texture::Texture2D(resolution, resolution, AE_DEPTH24,
				GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
			framebuffer->AddComponentTexture(GL_DEPTH_ATTACHMENT, depthTexture);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT1, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT2, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT3, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
            */

			auto impostor = new Mesh::Impostor(views, resolution);

			//framebuffer->Unbind();

			std::vector<mat4> viewMatrices;

			auto min = mesh->data.aabb.min;
			auto max = mesh->data.aabb.max;

			auto center = min * 0.5f + max * 0.5f;
			auto radius = glm::distance(center, max);	

			auto dist = 10.0f * radius;
			float fov = 2.0f * atanf(radius / dist);

			std::vector<vec3> rightVectors;
			std::vector<vec3> upVectors;

			for (int32_t i = 0; i < views * views; i++) {

				int32_t x = i % views;
				int32_t y = i / views;

				vec2 coord = vec2((float)x, (float)y) / vec2((float)(views - 1));
				vec3 dir;
				if (octahedron) {
					dir = OctahedronToUnitVector(coord);
				}
				else {
					dir = HemiOctahedronToUnitVector(coord);
				}

				auto eye = dir * dist + center;

				vec3 up = vec3(0.0f, 1.0f, 0.0f);
				vec3 right = normalize(cross(up, dir));
				up = normalize(cross(dir, right));

				auto matrix = glm::lookAt(eye,
					eye - dir, vec3(0.0f, 1.0f, 0.0f));

				rightVectors.push_back(right);
				upVectors.push_back(up);

				viewMatrices.push_back(matrix);

			}

			mat4 projectionMatrix;

			if (orthoProjection) {

			}
			else {
				projectionMatrix = glm::perspective(fov,
					1.0f, 1.0f, dist + 2.0f * radius);
			}

			//renderer.RenderImpostor(&viewport, framebuffer, viewMatrices,
			//	projectionMatrix, mesh, impostor);

			impostor->center = center;
			impostor->radius = radius;

			impostor->FillViewPlaneBuffer(rightVectors, upVectors);

			// Approximate transmission
			for (auto& material : mesh->data.materials) {
				impostor->transmissiveColor += material.transmissiveColor / (float)mesh->data.materials.size();
			}

			//delete framebuffer;

			impostor->baseColorTexture.GenerateMipmap();
			impostor->roughnessMetalnessAoTexture.GenerateMipmap();
			impostor->normalTexture.GenerateMipmap();

			return impostor;

		}

		vec3 ImpostorTool::HemiOctahedronToUnitVector(vec2 coord) {

			coord = 2.0f * coord - 1.0f;
			coord = 0.5f * vec2(coord.x + coord.y, coord.x - coord.y);

			auto y = 1.0f - glm::dot(vec2(1.0), glm::abs(coord));

			return normalize(vec3(coord.x, y + 0.0001f, coord.y));

		}

		vec2 ImpostorTool::UnitVectorToHemiOctahedron(vec3 dir) {

			dir.y = glm::max(dir.y, 0.0001f);
			dir /= glm::dot(glm::abs(dir), vec3(1.0));

			return glm::clamp(0.5f * vec2(dir.x + dir.z, dir.x - dir.z) + 0.5f, 0.0f, 1.0f);

		}

		vec3 ImpostorTool::OctahedronToUnitVector(vec2 coord) {

			coord = 2.0f * coord - 1.0f;
			auto y = 1.0f - dot(glm::abs(coord), vec2(1.0f));

			// Lower hemisphere
			if (y < 0.0f) {
				vec2 orig = coord;
				coord.x = (orig.x >= 0.0f ? 1.0f : -1.0f) * (1.0f - abs(orig.y));
				coord.y = (orig.y >= 0.0f ? 1.0f : -1.0f) * (1.0f - abs(orig.x));
			}
			
			return normalize(vec3(coord.x, y + 0.0001f, coord.y));

		}

		vec2 ImpostorTool::UnitVectorToOctahedron(vec3 dir) {

			dir /= glm::dot(glm::abs(dir), vec3(1.0));

			// Lower hemisphere
			if (dir.y < 0.0f) {
				vec2 orig = vec2(dir.x, dir.z);
				dir.x = (orig.x >= 0.0f ? 1.0f : -1.0f) * (1.0f - abs(orig.y));
				dir.z = (orig.y >= 0.0f ? 1.0f : -1.0f) * (1.0f - abs(orig.x));
			}

			return glm::clamp(0.5f * vec2(dir.x, dir.z) + 0.5f, 0.0f, 1.0f);

		}

	}

}