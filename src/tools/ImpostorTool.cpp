#include "ImpostorTool.h"
#include "../RenderTarget.h"
#include "../renderer/OpaqueRenderer.h"
#include "../scene/Scene.h"

namespace Atlas {

	namespace Tools {

		Mesh::Impostor* ImpostorTool::GenerateImpostor(Mesh::Mesh* mesh, int32_t resolution) {

			Renderer::OpaqueRenderer renderer;
			Viewport viewport(0, 0, resolution, resolution);
			Camera camera;

			auto framebuffer = new Framebuffer(resolution, resolution);
			auto depthTexture = new Texture::Texture2D(resolution, resolution, AE_DEPTH24,
				GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
			framebuffer->AddComponentTexture(GL_DEPTH_ATTACHMENT, depthTexture);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT1, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);

			auto impostor = new Mesh::Impostor(1, resolution);

			auto min = mesh->data.aabb.min;
			auto max = mesh->data.aabb.max;
			auto center = min * 0.5f + max * 0.5f;
			auto eye = vec3(center.x, center.y, max.z);

			camera.viewMatrix = glm::lookAt(vec3(0.0f, 0.0f, 0.0f),
				vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 1.0f, 0.0f));

			auto minProj = 0.0f, maxProj = 0.0f;

			// Prevent stretching on one axis
			auto diff = max - min;

			if (diff.x > diff.y) {
				auto offset = diff.x - diff.y;
				max.y += offset / 2.0f;
				min.y -= offset / 2.0f;
			}
			else {
				auto offset = diff.y - diff.x;
				max.x += offset / 2.0f;
				min.x -= offset / 2.0f;
			}
			
			camera.projectionMatrix = glm::ortho(min.x, max.x,
				min.y, max.y, -100.0f, 100.0f);

			framebuffer->Unbind();

			renderer.RenderImpostor(&viewport, framebuffer, &camera, mesh);

			impostor->aabb = Volume::AABB(
				vec3(min.x, min.y, 0.0f),
				vec3(max.x, max.y, max.z - min.z)
			);

			impostor->cameraMatrix = camera.viewMatrix;

			impostor->diffuseTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT0),
				0, 0, 0, 0, 0, 0, resolution, resolution, 1);
			impostor->normalTexture.Copy(*framebuffer->GetComponentTexture(GL_COLOR_ATTACHMENT1),
				0, 0, 0, 0, 0, 0, resolution, resolution, 1);

			delete framebuffer;

			impostor->diffuseTexture.GenerateMipmap();
			impostor->normalTexture.GenerateMipmap();

			return impostor;

		}

	}

}