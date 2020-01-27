#include "ImpostorTool.h"
#include "../RenderTarget.h"
#include "../renderer/OpaqueRenderer.h"
#include "../scene/Scene.h"

namespace Atlas {

	namespace Tools {

		Mesh::Impostor* ImpostorTool::GenerateImpostor(Mesh::Mesh* mesh, Camera* camera,
			int32_t views, int32_t resolution) {

			// https://www.gamasutra.com/view/feature/130911/dynamic_2d_imposters_a_simple_.php?page=2
			/*
			Renderer::OpaqueRenderer renderer;
			Viewport viewport(0, 0, resolution, resolution);

			auto framebuffer = new Framebuffer(resolution, resolution);
			auto depthTexture = new Texture::Texture2D(resolution, resolution, AE_DEPTH24,
				GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
			framebuffer->AddComponentTexture(GL_DEPTH_ATTACHMENT, depthTexture);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT1, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT2, AE_RG16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

			auto impostor = new Mesh::Impostor(views, resolution);

			framebuffer->Unbind();

			std::vector<mat4> viewMatrices;
			std::vector<Volume::AABB> aabbs;

			auto min = mesh->data.aabb.min;
			auto max = mesh->data.aabb.max;

			for (int32_t i = 0; i < views; i++) {

				auto z = cosf((float)i / (float)views * 2.0f * 3.14159265f);
				auto x = sinf((float)i / (float)views * 2.0f * 3.14159265f);

				auto dir = glm::normalize(vec3(x, 0.0f, z));

				auto matrix = glm::lookAt(vec3(0.0f, 0.0f, 0.0f),
					dir, vec3(0.0f, 1.0f, 0.0f));

				auto aabb = mesh->data.aabb.Transform(matrix);
				aabbs.push_back(aabb);

				min = glm::min(min, aabb.min);
				max = glm::max(max, aabb.max);

			}

			// Prevent stretching on one axis
			auto diff = max - min;

			float radians = glm::radians(camera->fieldOfView);
			float tang = tanf(radians);			

			auto dim = diff.x > diff.y ? diff.x : diff.y;

			auto dist = dim / tang;

			for (int32_t i = 0; i < views; i++) {

				auto z = cosf((float)i / (float)views * 2.0f * 3.14159265f);
				auto x = sinf((float)i / (float)views * 2.0f * 3.14159265f);

				auto min = aabbs[i].min;
				auto diff = aabbs[i].max - aabbs[i].min;
				diff.z = 0.0f;

				auto dir = glm::normalize(vec3(x, 0.0f, z));
				auto eye = -dir * dist + min + diff * 0.5f + dir;

				auto matrix = glm::lookAt(eye,
					eye + dir, vec3(0.0f, 1.0f, 0.0f));

				viewMatrices.push_back(matrix);

			}			

			auto center = min * 0.5f + max * 0.5f;
			auto eye = vec3(center.x, center.y, max.z);		

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

			auto projectionMatrix = glm::perspective(glm::radians(camera->fieldOfView), 
				1.0f, camera->nearPlane, dist + (max.z - min.z));

			renderer.RenderImpostor(&viewport, framebuffer, viewMatrices, 
				projectionMatrix, mesh, impostor);

			impostor->aabb = Volume::AABB(
				vec3(min.x, min.y, 0.0f),
				vec3(max.x, max.y, max.z - min.z)
			);

			delete framebuffer;

			impostor->diffuseTexture.GenerateMipmap();
			impostor->normalTexture.GenerateMipmap();
			impostor->specularTexture.GenerateMipmap();

			return impostor;
			*/
			Renderer::OpaqueRenderer renderer;
			Viewport viewport(0, 0, resolution, resolution);

			auto framebuffer = new Framebuffer(resolution, resolution);
			auto depthTexture = new Texture::Texture2D(resolution, resolution, AE_DEPTH24,
				GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
			framebuffer->AddComponentTexture(GL_DEPTH_ATTACHMENT, depthTexture);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT0, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT1, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			framebuffer->AddComponent(GL_COLOR_ATTACHMENT2, AE_RG16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

			auto impostor = new Mesh::Impostor(views, resolution);

			auto min = mesh->data.aabb.min;
			auto max = mesh->data.aabb.max;
			auto center = min * 0.5f + max * 0.5f;
			auto eye = vec3(center.x, center.y, max.z);

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

			auto projectionMatrix = glm::ortho(min.x, max.x,
				min.y, max.y, -glm::length(min), glm::length(max));

			framebuffer->Unbind();

			std::vector<mat4> viewMatrices;

			for (int32_t i = 0; i < views; i++) {

				auto z = cosf((float)i / (float)views * 2.0f * 3.14159265f);
				auto x = sinf((float)i / (float)views * 2.0f * 3.14159265f);

				auto dir = glm::normalize(vec3(x, 0.0f, z));

				viewMatrices.push_back(glm::lookAt(vec3(0.0f, 0.0f, 0.0f),
					dir, vec3(0.0f, 1.0f, 0.0f)));

			}

			renderer.RenderImpostor(&viewport, framebuffer, viewMatrices,
				projectionMatrix, mesh, impostor);

			impostor->aabb = Volume::AABB(
				vec3(min.x, min.y, 0.0f),
				vec3(max.x, max.y, max.z - min.z)
			);

			delete framebuffer;

			impostor->diffuseTexture.GenerateMipmap();
			impostor->normalTexture.GenerateMipmap();
			impostor->specularTexture.GenerateMipmap();

			return impostor;

		}

	}

}