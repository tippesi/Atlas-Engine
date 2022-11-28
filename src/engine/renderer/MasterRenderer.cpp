#include "MasterRenderer.h"
#include "helper/GeometryHelper.h"
#include "helper/HaltonSequence.h"

#include "../common/Packing.h"

#define FEATURE_BASE_COLOR_MAP (1 << 1)
#define FEATURE_OPACITY_MAP (1 << 2)
#define FEATURE_NORMAL_MAP (1 << 3)
#define FEATURE_ROUGHNESS_MAP (1 << 4)
#define FEATURE_METALNESS_MAP (1 << 5)
#define FEATURE_AO_MAP (1 << 6)
#define FEATURE_TRANSMISSION (1 << 7)

namespace Atlas {

	namespace Renderer {

		MasterRenderer::MasterRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);
			Helper::GeometryHelper::GenerateCubeVertexArray(cubeVertexArray);

			rectangleShader.AddStage(AE_VERTEX_STAGE, "rectangle.vsh");
			rectangleShader.AddStage(AE_FRAGMENT_STAGE, "rectangle.fsh");

			rectangleShader.Compile();

			texture2DShader.AddStage(AE_VERTEX_STAGE, "rectangle.vsh");
			texture2DShader.AddStage(AE_FRAGMENT_STAGE, "rectangle.fsh");

			texture2DShader.AddMacro("TEXTURE2D");

			texture2DShader.Compile();

			texture2DArrayShader.AddStage(AE_VERTEX_STAGE, "rectangle.vsh");
			texture2DArrayShader.AddStage(AE_FRAGMENT_STAGE, "rectangle.fsh");

			texture2DArrayShader.AddMacro("TEXTURE2D_ARRAY");

			texture2DArrayShader.Compile();

			lineShader.AddStage(AE_VERTEX_STAGE, "primitive.vsh");
			lineShader.AddStage(AE_FRAGMENT_STAGE, "primitive.fsh");

			GetUniforms();

			createProbeFaceShader.AddStage(AE_COMPUTE_STAGE, "brdf/createProbeFace.csh");

			filterDiffuseShader.AddStage(AE_VERTEX_STAGE, "brdf/filterProbe.vsh");
			filterDiffuseShader.AddStage(AE_FRAGMENT_STAGE, "brdf/filterProbe.fsh");

			haltonSequence = Helper::HaltonSequence::Generate(2, 3, 16 + 1);

			PreintegrateBRDF();

		}

		MasterRenderer::~MasterRenderer() {

			

		}

		void MasterRenderer::RenderScene(Viewport* viewport, RenderTarget* target, Camera* camera, 
			Scene::Scene* scene, Texture::Texture2D* texture, RenderBatch* batch) {

			Profiler::BeginQuery("Render scene");

			glDisable(GL_DEPTH_TEST);
			glDisable(GL_CULL_FACE);

			if (scene->vegetation)
				vegetationRenderer.helper.PrepareInstanceBuffer(*scene->vegetation, camera);

			glEnable(GL_DEPTH_TEST);
			glEnable(GL_CULL_FACE);

			std::vector<PackedMaterial> materials;
			std::unordered_map<void*, uint16_t> materialMap;

			PrepareMaterials(scene, materials, materialMap);

			dfgPreintegrationTexture.Bind(31);

			auto materialBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(PackedMaterial), 0,
				materials.size(), materials.data());

			auto& taa = scene->postProcessing.taa;
			if (taa.enable) {
				auto jitter = 2.0f * haltonSequence[haltonIndex] - 1.0f;
				jitter.x /= (float)target->GetWidth();
				jitter.y /= (float)target->GetHeight();

				camera->Jitter(jitter * taa.jitterRange);
			}
			else {
				// Even if there is no TAA we need to update the jitter for other techniques
				// E.g. the reflections and ambient occlusion use reprojection
				camera->Jitter(vec2(0.0f));
			}

			if (scene->sky.probe) {
				if (scene->sky.probe->update) {
					scene->sky.probe->filteredDiffuse.Bind(0);
					FilterProbe(scene->sky.probe);
					scene->sky.probe->update = false;
				}
			}

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			// Clear the lights depth maps
			depthFramebuffer.Bind();

			auto lights = scene->GetLights();

			for (auto light : lights) {

				if (!light->GetShadow())
					continue;
				if (!light->GetShadow()->update)
					continue;

				for (int32_t i = 0; i < light->GetShadow()->componentCount; i++) {
					if (light->GetShadow()->useCubemap) {
						depthFramebuffer.AddComponentCubemap(GL_DEPTH_ATTACHMENT,
							&light->GetShadow()->cubemap, i);
					}
					else {
						depthFramebuffer.AddComponentTextureArray(GL_DEPTH_ATTACHMENT,
							&light->GetShadow()->maps, i);
					}

					glClear(GL_DEPTH_BUFFER_BIT);
				}
			}

			shadowRenderer.Render(viewport, target, camera, scene);
			
			glEnable(GL_CULL_FACE);

			terrainShadowRenderer.Render(viewport, target, camera, scene);

			glCullFace(GL_BACK);

			// Shadows have been updated
			for (auto light : lights) {
				if (!light->GetShadow())
					continue;
				light->GetShadow()->update = false;
			}

			ddgiRenderer.TraceAndUpdateProbes(scene);

			materialBuffer.BindBase(16);

			target->geometryFramebuffer.Bind(true);
			target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
				GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6 });

			glEnable(GL_CULL_FACE);

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

			opaqueRenderer.Render(viewport, target, camera, scene, materialMap);

			ddgiRenderer.DebugProbes(viewport, target, camera, scene, materialMap);

			vegetationRenderer.Render(viewport, target, camera, scene, materialMap);

			terrainRenderer.Render(viewport, target, camera, scene, materialMap);

			glEnable(GL_CULL_FACE);
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			downscaleRenderer.Downscale(target);

			target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });

			decalRenderer.Render(viewport, target, camera, scene);

			glDisable(GL_BLEND);

			aoRenderer.Render(viewport, target, camera, scene);
			rtrRenderer.Render(viewport, target, camera, scene);

			vertexArray.Bind();

			target->lightingFramebuffer.Bind(true);
			target->lightingFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0 });

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			directionalLightRenderer.Render(viewport, target, camera, scene);

			indirectLightRenderer.Render(viewport, target, camera, scene);

			glEnable(GL_DEPTH_TEST);

			target->lightingFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
				GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });

			if (batch) {
				glDepthMask(GL_TRUE);
				RenderBatched(nullptr, camera, batch);
				glDepthMask(GL_FALSE);
			}

			if (scene->sky.probe) {
				skyboxRenderer.Render(viewport, target, camera, scene);
			}
			else {
				atmosphereRenderer.Render(viewport, target, camera, scene);
			}

			glDepthMask(GL_TRUE);

			if (scene->ocean) {
				oceanRenderer.Render(viewport, target, camera, scene);

				downscaleRenderer.DownscaleDepthOnly(target);
			}

			glDisable(GL_DEPTH_TEST);

			glDisable(GL_CULL_FACE);
			glCullFace(GL_BACK);
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);

			vertexArray.Bind();

			volumetricRenderer.Render(viewport, target, camera, scene);

			if (taa.enable) {
				taaRenderer.Render(viewport, target, camera, scene);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}
			else {
				target->lightingFramebuffer.Unbind();
			}

			// Swap history and current textures
			target->Swap();

			vertexArray.Bind();

			if (texture) {
				framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, texture);
				framebuffer.Bind();
			}

			postProcessRenderer.Render(viewport, target, camera, scene);

			Atlas::Texture::Texture2D* postTex;

			auto& sharpen = scene->postProcessing.sharpen;
			if (sharpen.enable) {
				postTex = &target->postProcessTexture;
				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}
			else {
				postTex = target->postProcessFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0);
			}

			if (texture) {
				framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, texture);
				RenderTexture(viewport, postTex, 0.0f, 0.0f,
					(float)viewport->width, (float)viewport->height,
					false, &framebuffer);
			}
			else {
				RenderTexture(viewport, postTex, 0.0f, 0.0f,
					(float)viewport->width, (float)viewport->height);
			}

			Profiler::EndQuery();

		}

		void MasterRenderer::RenderTexture(Viewport* viewport, Texture::Texture2D* texture, float x, float y, float width, float height,
			bool alphaBlending, bool invert, Framebuffer* framebuffer) {

			float viewportWidth = (float)viewport->width;
			float viewportHeight = (float)viewport->height;

			vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
			vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

			RenderTexture(viewport, texture, x, y, width, height, clipArea, blendArea, alphaBlending, invert, framebuffer);

		}

		void MasterRenderer::RenderTexture(Viewport* viewport, Texture::Texture2D* texture, float x, float y, float width, float height,
			vec4 clipArea, vec4 blendArea, bool alphaBlending, bool invert, Framebuffer* framebuffer) {

			vertexArray.Bind();

			texture2DShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer)
				framebuffer->Bind();

			glViewport(0, 0, viewport->width, viewport->height);

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			texture2DProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewport->width, 0.0f, (float)viewport->height));
			texture2DOffset->SetValue(vec2(x, y));
			texture2DScale->SetValue(vec2(width, height));
			texture2DBlendArea->SetValue(blendArea);
			texture2DClipArea->SetValue(clipArea);
			texture2DShader.GetUniform("invert")->SetValue(invert);

			texture->Bind(0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending)
				glDisable(GL_BLEND);

			if (framebuffer)
				framebuffer->Unbind();

			glEnable(GL_CULL_FACE);

		}

		void MasterRenderer::RenderTexture(Viewport* viewport, Texture::Texture2DArray* texture, int32_t depth, float x,
			float y, float width, float height,  bool alphaBlending, bool invert, Framebuffer* framebuffer) {

			float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
			float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

			vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
			vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

			RenderTexture(viewport, texture, depth, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);

		}

		void MasterRenderer::RenderTexture(Viewport* viewport, Texture::Texture2DArray* texture, int32_t depth, float x, float y, float width, float height,
			vec4 clipArea, vec4 blendArea, bool alphaBlending, bool invert, Framebuffer* framebuffer) {

			vertexArray.Bind();

			texture2DArrayShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, viewport->width, viewport->height);
			}

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}


			float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
			float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

			texture2DArrayProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight));
			texture2DArrayOffset->SetValue(vec2(x, y));
			texture2DArrayScale->SetValue(vec2(width, height));
			texture2DArrayBlendArea->SetValue(blendArea);
			texture2DArrayClipArea->SetValue(clipArea);
			texture2DArrayDepth->SetValue((float)depth);
			texture2DArrayShader.GetUniform("invert")->SetValue(invert);

			texture->Bind(0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer) {
				framebuffer->Unbind();
			}

			glEnable(GL_CULL_FACE);

		}

		void MasterRenderer::RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
			bool alphaBlending, Framebuffer* framebuffer) {

			float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
			float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

			if (x > viewportWidth || y > viewportHeight ||
				y + height < 0 || x + width < 0) {
				return;
			}

			vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
			vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

			RenderRectangle(viewport, color, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);

		}

		void MasterRenderer::RenderRectangle(Viewport* viewport, vec4 color, float x, float y, float width, float height,
			vec4 clipArea, vec4 blendArea, bool alphaBlending, Framebuffer* framebuffer) {

			float viewportWidth = (float)(!framebuffer ? viewport->width : framebuffer->width);
			float viewportHeight = (float)(!framebuffer ? viewport->height : framebuffer->height);

			if (x > viewportWidth || y > viewportHeight ||
				y + height < 0 || x + width < 0) {
				return;
			}

			vertexArray.Bind();

			rectangleShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, viewport->width, viewport->height);
			}

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}

			rectangleProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight));
			rectangleOffset->SetValue(vec2(x, y));
			rectangleScale->SetValue(vec2(width, height));
			rectangleColor->SetValue(color);
			rectangleBlendArea->SetValue(blendArea);
			rectangleClipArea->SetValue(clipArea);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer) {
				framebuffer->Unbind();
			}

			glEnable(GL_CULL_FACE);

		}

		void MasterRenderer::RenderBatched(Viewport* viewport, Camera* camera, RenderBatch* batch) {

			batch->TransferData();

			if (viewport)
				glViewport(viewport->x, viewport->y,
					viewport->width, viewport->height);

			lineShader.Bind();

			lineViewMatrix->SetValue(camera->viewMatrix);
			lineProjectionMatrix->SetValue(camera->projectionMatrix);

			if (batch->GetLineCount()) {

				glLineWidth(batch->GetLineWidth());

				batch->BindLineBuffer();

				glDrawArrays(GL_LINES, 0, (GLsizei)batch->GetLineCount() * 2);

				glLineWidth(1.0f);

			}

			if (batch->GetTriangleCount()) {
				batch->BindTriangleBuffer();

				glDrawArrays(GL_TRIANGLES, 0, GLsizei(batch->GetTriangleCount() * 3));
			}

		}

		void MasterRenderer::RenderProbe(Lighting::EnvironmentProbe* probe, RenderTarget* target, Scene::Scene* scene) {

		    if (probe->resolution != target->GetWidth() ||
		        probe->resolution != target->GetHeight())
		        return;

			std::vector<PackedMaterial> materials;
			std::unordered_map<void*, uint16_t> materialMap;
			Viewport viewport(0, 0, probe->resolution, probe->resolution);

			PrepareMaterials(scene, materials, materialMap);

			auto materialBuffer = Buffer::Buffer(AE_SHADER_STORAGE_BUFFER, sizeof(PackedMaterial), 0,
				materials.size(), materials.data());			

			Lighting::EnvironmentProbe* skyProbe = nullptr;

			if (scene->sky.probe) {
				skyProbe = scene->sky.probe;
				scene->sky.probe = nullptr;
			}

			vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
							 vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
							 vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

			vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
						   vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
						   vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

			Camera camera(90.0f, 1.0f, 0.5f, 1000.0f);
			camera.UpdateProjection();

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			for (uint8_t i = 0; i < 6; i++) {

				vec3 dir = faces[i];
				vec3 up = ups[i];
				vec3 right = normalize(cross(up, dir));
				up = normalize(cross(dir, right));

				camera.viewMatrix = glm::lookAt(probe->GetPosition(), probe->GetPosition() + dir, up);
				camera.invViewMatrix = glm::inverse(camera.viewMatrix);
				camera.location = probe->GetPosition();
				camera.direction = dir;
				camera.right = right;
				camera.up = up;

				camera.frustum = Volume::Frustum(camera.projectionMatrix * camera.viewMatrix);

				scene->Update(&camera, 0.0f);

				// Clear the lights depth maps
				depthFramebuffer.Bind();

				auto lights = scene->GetLights();

				for (auto light : lights) {

					if (!light->GetShadow())
						continue;
					if (!light->GetShadow()->update)
						continue;

					for (int32_t j = 0; j < light->GetShadow()->componentCount; j++) {
						if (light->GetShadow()->useCubemap) {
							depthFramebuffer.AddComponentCubemap(GL_DEPTH_ATTACHMENT,
								&light->GetShadow()->cubemap, j);
						}
						else {
							depthFramebuffer.AddComponentTextureArray(GL_DEPTH_ATTACHMENT,
								&light->GetShadow()->maps, j);
						}

						glClear(GL_DEPTH_BUFFER_BIT);
					}
				}

				shadowRenderer.Render(&viewport, target, &camera, scene);

				glEnable(GL_CULL_FACE);

				glCullFace(GL_FRONT);

				terrainShadowRenderer.Render(&viewport, target, &camera, scene);

				glCullFace(GL_BACK);

				// Shadows have been updated
				for (auto light : lights) {
					if (!light->GetShadow())
						continue;
					light->GetShadow()->update = false;
				}

				materialBuffer.BindBase(0);

				target->geometryFramebuffer.Bind(true);
				target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
					GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3,
					GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5 });

				glEnable(GL_CULL_FACE);

				glClear(GL_DEPTH_BUFFER_BIT | GL_COLOR_BUFFER_BIT);

				opaqueRenderer.Render(&viewport, target, &camera, scene, materialMap);

				terrainRenderer.Render(&viewport, target, &camera, scene, materialMap);

				glEnable(GL_CULL_FACE);
				glDepthMask(GL_FALSE);
				glDisable(GL_DEPTH_TEST);
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				target->geometryFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
					GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 });

				decalRenderer.Render(&viewport, target, &camera, scene);

				glDisable(GL_BLEND);

				vertexArray.Bind();

				target->lightingFramebuffer.Bind(true);
				target->lightingFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0 });

				glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
				glClear(GL_COLOR_BUFFER_BIT);

				directionalLightRenderer.Render(&viewport, target, &camera, scene);

				glEnable(GL_DEPTH_TEST);

				target->lightingFramebuffer.SetDrawBuffers({ GL_COLOR_ATTACHMENT0,
					GL_COLOR_ATTACHMENT1 });

				glDepthMask(GL_TRUE);

				oceanRenderer.Render(&viewport, target, &camera, scene);

				glDisable(GL_CULL_FACE);
				glCullFace(GL_BACK);
				glDepthMask(GL_FALSE);
				glDisable(GL_DEPTH_TEST);

				vertexArray.Bind();

				volumetricRenderer.Render(&viewport, target, &camera, scene);

				createProbeFaceShader.Bind();

				createProbeFaceShader.GetUniform("faceIndex")->SetValue((int32_t)i);
				createProbeFaceShader.GetUniform("ipMatrix")->SetValue(camera.invProjectionMatrix);

				int32_t groupCount = probe->resolution / 8;
				groupCount += ((groupCount * 8 == probe->resolution) ? 0 : 1);

				probe->cubemap.Bind(GL_WRITE_ONLY, 0);
				probe->depth.Bind(GL_WRITE_ONLY, 1);

				target->lightingFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT0)->Bind(0);
				target->lightingFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT)->Bind(1);

				glDispatchCompute(groupCount, groupCount, 1);

				glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			}

			if (skyProbe) {
				scene->sky.probe = skyProbe;
			}

		}

		void MasterRenderer::FilterProbe(Lighting::EnvironmentProbe* probe) {

			mat4 projectionMatrix = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
			vec3 faces[] = { vec3(1.0f, 0.0f, 0.0f), vec3(-1.0f, 0.0f, 0.0f),
							 vec3(0.0f, 1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
							 vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f) };

			vec3 ups[] = { vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f),
						   vec3(0.0f, 0.0f, 1.0f), vec3(0.0f, 0.0f, -1.0f),
						   vec3(0.0f, -1.0f, 0.0f), vec3(0.0f, -1.0f, 0.0f) };

			Framebuffer framebuffer;
			filterDiffuseShader.Bind();

			auto matrixUniform = filterDiffuseShader.GetUniform("pvMatrix");

			cubeVertexArray.Bind();
			framebuffer.Bind();

			probe->cubemap.Bind(0);

			glViewport(0, 0, probe->filteredDiffuse.width, probe->filteredDiffuse.height);
			glDisable(GL_DEPTH_TEST);

			for (uint8_t i = 0; i < 6; i++) {
				auto matrix = projectionMatrix *
					mat4(mat3(glm::lookAt(vec3(0.0f), faces[i], ups[i])));

				matrixUniform->SetValue(matrix);

				framebuffer.AddComponentCubemap(GL_COLOR_ATTACHMENT0, &probe->filteredDiffuse, i);
				glClear(GL_COLOR_BUFFER_BIT);

				glDrawArrays(GL_TRIANGLES, 0, 36);
			}

			glEnable(GL_DEPTH_TEST);
			framebuffer.Unbind();

		}

		void MasterRenderer::Update() {

			static auto framecounter = 0;

			textRenderer.Update();

			haltonIndex = (haltonIndex + 1) % haltonSequence.size();

		}

		void MasterRenderer::GetUniforms() {

			rectangleProjectionMatrix = rectangleShader.GetUniform("pMatrix");
			rectangleOffset = rectangleShader.GetUniform("rectangleOffset");
			rectangleScale = rectangleShader.GetUniform("rectangleScale");
			rectangleColor = rectangleShader.GetUniform("rectangleColor");
			rectangleBlendArea = rectangleShader.GetUniform("rectangleBlendArea");
			rectangleClipArea = rectangleShader.GetUniform("rectangleClipArea");

			texture2DProjectionMatrix = texture2DShader.GetUniform("pMatrix");
			texture2DOffset = texture2DShader.GetUniform("rectangleOffset");
			texture2DScale = texture2DShader.GetUniform("rectangleScale");
			texture2DBlendArea = texture2DShader.GetUniform("rectangleBlendArea");
			texture2DClipArea = texture2DShader.GetUniform("rectangleClipArea");

			texture2DArrayProjectionMatrix = texture2DArrayShader.GetUniform("pMatrix");
			texture2DArrayOffset = texture2DArrayShader.GetUniform("rectangleOffset");
			texture2DArrayScale = texture2DArrayShader.GetUniform("rectangleScale");
			texture2DArrayBlendArea = texture2DArrayShader.GetUniform("rectangleBlendArea");
			texture2DArrayClipArea = texture2DArrayShader.GetUniform("rectangleClipArea");
			texture2DArrayDepth = texture2DArrayShader.GetUniform("textureDepth");

			lineViewMatrix = lineShader.GetUniform("vMatrix");
			lineProjectionMatrix = lineShader.GetUniform("pMatrix");

		}

		void MasterRenderer::PrepareMaterials(Scene::Scene* scene, std::vector<PackedMaterial>& materials,
			std::unordered_map<void*, uint16_t>& materialMap) {

			auto sceneMaterials = scene->GetMaterials();

			// For debugging purpose
			if (scene->irradianceVolume && scene->irradianceVolume->debug) {
				sceneMaterials.push_back(&ddgiRenderer.probeDebugMaterial);
				sceneMaterials.push_back(&ddgiRenderer.probeDebugActiveMaterial);
				sceneMaterials.push_back(&ddgiRenderer.probeDebugInactiveMaterial);
				sceneMaterials.push_back(&ddgiRenderer.probeDebugOffsetMaterial);
			}

			uint16_t idx = 0;

			for (auto material : sceneMaterials) {
				PackedMaterial packed;

				auto emissiveIntensity = glm::max(glm::max(material->emissiveColor.r,
					material->emissiveColor.g), material->emissiveColor.b);

				packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(material->baseColor, 0.0f));
				packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(material->emissiveColor / emissiveIntensity, 0.0f));
				packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(material->transmissiveColor, 0.0f));

				packed.emissiveIntensityTiling = glm::packHalf2x16(vec2(emissiveIntensity, material->tiling));

				vec4 data0, data1, data2;

				data0.x = material->opacity;
				data0.y = material->roughness;
				data0.z = material->metalness;

				data1.x = material->ao;
				data1.y = material->HasNormalMap() ? material->normalScale : 0.0f;
				data1.z = material->HasDisplacementMap() ? material->displacementScale : 0.0f;

				data2.x = material->reflectance;
				// Note used
				data2.y = 0.0f;
				data2.z = 0.0f;

				packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
				packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);
				packed.data2 = Common::Packing::PackUnsignedVector3x10_1x2(data2);

				packed.features = 0;

				packed.features |= material->HasBaseColorMap() ? FEATURE_BASE_COLOR_MAP : 0;
				packed.features |= material->HasOpacityMap() ? FEATURE_OPACITY_MAP : 0;
				packed.features |= material->HasNormalMap() ? FEATURE_NORMAL_MAP : 0;
				packed.features |= material->HasRoughnessMap() ? FEATURE_ROUGHNESS_MAP : 0;
				packed.features |= material->HasMetalnessMap() ? FEATURE_METALNESS_MAP : 0;
				packed.features |= material->HasAoMap() ? FEATURE_AO_MAP : 0;
				packed.features |= glm::length(material->transmissiveColor) > 0.0f ? FEATURE_TRANSMISSION : 0;

				materials.push_back(packed);

				materialMap[material] = idx++;
			}
			
			auto meshes = scene->GetMeshes();

			for (auto mesh : meshes) {
				auto impostor = mesh->impostor;

				if (!impostor)
					continue;

				PackedMaterial packed;

				packed.baseColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(1.0f));
				packed.emissiveColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(0.0f));
				packed.transmissionColor = Common::Packing::PackUnsignedVector3x10_1x2(vec4(impostor->transmissiveColor, 1.0f));

				vec4 data0, data1, data2;

				data0.x = 1.0f;
				data0.y = 1.0f;
				data0.z = 1.0f;

				data1.x = 1.0f;
				data1.y = 0.0f;
				data1.z = 0.0f;

				data2.x = 0.5f;
				// Note used
				data2.y = 0.0f;
				data2.z = 0.0f;

				packed.data0 = Common::Packing::PackUnsignedVector3x10_1x2(data0);
				packed.data1 = Common::Packing::PackUnsignedVector3x10_1x2(data1);
				packed.data2 = Common::Packing::PackUnsignedVector3x10_1x2(data2);

				packed.features = 0;

				packed.features |= FEATURE_BASE_COLOR_MAP | 
					FEATURE_ROUGHNESS_MAP | FEATURE_METALNESS_MAP | FEATURE_AO_MAP;
				packed.features |= glm::length(impostor->transmissiveColor) > 0.0f ? FEATURE_TRANSMISSION : 0;

				materials.push_back(packed);

				materialMap[impostor] =  idx++;
			}
			

		}

		void MasterRenderer::PreintegrateBRDF() {

			Shader::Shader shader;

			shader.AddStage(AE_COMPUTE_STAGE, "brdf/preintegrateDFG.csh");

			shader.Compile();

			const int32_t res = 256;
			dfgPreintegrationTexture = Texture::Texture2D(res, res, AE_RGBA16F);

			int32_t groupCount = res / 8;
			groupCount += ((res % groupCount) ? 1 : 0);

			dfgPreintegrationTexture.Bind(GL_WRITE_ONLY, 0);

			shader.Bind();

			glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

			glDispatchCompute(groupCount, groupCount, 1);

			glFlush();

		}

	}

}