#include "MasterRenderer.h"
#include "helper/GeometryHelper.h"

namespace Atlas {

	namespace Renderer {

		std::string MasterRenderer::vertexPath = "rectangle.vsh";
		std::string MasterRenderer::fragmentPath = "rectangle.fsh";

		MasterRenderer::MasterRenderer() {

			Helper::GeometryHelper::GenerateRectangleVertexArray(vertexArray);

			rectangleShader.AddStage(AE_VERTEX_STAGE, vertexPath);
			rectangleShader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			rectangleShader.Compile();

			texture2DShader.AddStage(AE_VERTEX_STAGE, vertexPath);
			texture2DShader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			texture2DShader.AddMacro("TEXTURE2D");

			texture2DShader.Compile();

			texture2DArrayShader.AddStage(AE_VERTEX_STAGE, vertexPath);
			texture2DArrayShader.AddStage(AE_FRAGMENT_STAGE, fragmentPath);

			texture2DArrayShader.AddMacro("TEXTURE2D_ARRAY");

			texture2DArrayShader.Compile();

			GetUniforms();

		}

		MasterRenderer::~MasterRenderer() {

			

		}

		void MasterRenderer::RenderScene(Window* window, RenderTarget* target, Camera* camera, Scene::Scene* scene) {

			glEnable(GL_DEPTH_TEST);
			glDepthMask(GL_TRUE);

			shadowRenderer.Render(window, target, camera, scene);

			target->geometryFramebuffer.Bind(true);

			glEnable(GL_CULL_FACE);

			glClear(GL_DEPTH_BUFFER_BIT);

			terrainRenderer.Render(window, target, camera, scene);

			opaqueRenderer.Render(window, target, camera, scene);

			glEnable(GL_CULL_FACE);
			glDepthMask(GL_FALSE);
			glDisable(GL_DEPTH_TEST);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

			decalRenderer.Render(window, target, camera, scene);

			glDisable(GL_BLEND);

			vertexArray.Bind();

			directionalVolumetricRenderer.Render(window, target, camera, scene);

			target->lightingFramebuffer.Bind(true);

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT);

			// Additive blending of light volumes
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);

			directionalLightRenderer.Render(window, target, camera, scene);

			pointLightRenderer.Render(window, target, camera, scene);

			glDisable(GL_BLEND);
			glEnable(GL_DEPTH_TEST);

			if (scene->sky.skybox != nullptr) {
				skyboxRenderer.Render(window, target, camera, scene);
			}
			else {
				atmosphereRenderer.Render(window, target, camera, scene);
			}

			target->lightingFramebuffer.Unbind();

			glDisable(GL_DEPTH_TEST);

			vertexArray.Bind();

			postProcessRenderer.Render(window, target, camera, scene);

		}

		void MasterRenderer::RenderTexture(Window* window, Texture::Texture2D* texture, float x, float y, float width, float height,
										   bool alphaBlending, Framebuffer* framebuffer) {

			float viewportWidth = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float viewportHeight = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
			vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

			RenderTexture(window, texture, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);

		}

		void MasterRenderer::RenderTexture(Window* window, Texture::Texture2D* texture, float x, float y, float width, float height,
										   vec4 clipArea, vec4 blendArea, bool alphaBlending, Framebuffer* framebuffer) {

			vertexArray.Bind();

			texture2DShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer != nullptr) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, window->viewport.width, window->viewport.height);
			}

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}


			float viewportWidth = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float viewportHeight = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			texture2DProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight));
			texture2DOffset->SetValue(vec2(x, y));
			texture2DScale->SetValue(vec2(width, height));
			texture2DBlendArea->SetValue(blendArea);
			texture2DClipArea->SetValue(clipArea);
			texture2DTexture->SetValue(0);

			texture->Bind(GL_TEXTURE0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer != nullptr) {
				framebuffer->Unbind();
			}

			glEnable(GL_CULL_FACE);

		}

		void MasterRenderer::RenderTexture(Window* window, Texture::Texture2DArray* texture, int32_t depth, float x, float y, float width, float height,
										   bool alphaBlending, Framebuffer* framebuffer) {

			float viewportWidth = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float viewportHeight = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
			vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

			RenderTexture(window, texture, depth, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);

		}

		void MasterRenderer::RenderTexture(Window* window, Texture::Texture2DArray* texture, int32_t depth, float x, float y, float width, float height,
										   vec4 clipArea, vec4 blendArea, bool alphaBlending, Framebuffer* framebuffer) {

			vertexArray.Bind();

			texture2DArrayShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer != nullptr) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, window->viewport.width, window->viewport.height);
			}

			if (alphaBlending) {
				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			}


			float viewportWidth = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float viewportHeight = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			texture2DArrayProjectionMatrix->SetValue(glm::ortho(0.0f, (float)viewportWidth, 0.0f, (float)viewportHeight));
			texture2DArrayOffset->SetValue(vec2(x, y));
			texture2DArrayScale->SetValue(vec2(width, height));
			texture2DArrayBlendArea->SetValue(blendArea);
			texture2DArrayClipArea->SetValue(clipArea);
			texture2DArrayDepth->SetValue((float)depth);
			texture2DArrayTexture->SetValue(0);

			texture->Bind(GL_TEXTURE0);

			glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

			if (alphaBlending) {
				glDisable(GL_BLEND);
			}

			if (framebuffer != nullptr) {
				framebuffer->Unbind();
			}

			glEnable(GL_CULL_FACE);

		}

		void MasterRenderer::RenderRectangle(Window* window, vec4 color, float x, float y, float width, float height,
											 bool alphaBlending, Framebuffer* framebuffer) {

			float viewportWidth = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float viewportHeight = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			if (x > viewportWidth || y > viewportHeight ||
				y + height < 0 || x + width < 0) {
				return;
			}

			vec4 clipArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);
			vec4 blendArea = vec4(0.0f, 0.0f, viewportWidth, viewportHeight);

			RenderRectangle(window, color, x, y, width, height, clipArea, blendArea, alphaBlending, framebuffer);

		}

		void MasterRenderer::RenderRectangle(Window* window, vec4 color, float x, float y, float width, float height,
											 vec4 clipArea, vec4 blendArea, bool alphaBlending, Framebuffer* framebuffer) {

			float viewportWidth = (float)(framebuffer == nullptr ? window->viewport.width : framebuffer->width);
			float viewportHeight = (float)(framebuffer == nullptr ? window->viewport.height : framebuffer->height);

			if (x > viewportWidth || y > viewportHeight ||
				y + height < 0 || x + width < 0) {
				return;
			}

			vertexArray.Bind();

			rectangleShader.Bind();

			glDisable(GL_CULL_FACE);

			if (framebuffer != nullptr) {
				framebuffer->Bind(true);
			}
			else {
				glViewport(0, 0, window->viewport.width, window->viewport.height);
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

			if (framebuffer != nullptr) {
				framebuffer->Unbind();
			}

			glEnable(GL_CULL_FACE);

		}

		void MasterRenderer::Update() {

			textRenderer.Update();

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
			texture2DTexture = texture2DShader.GetUniform("rectangleTexture");
			texture2DBlendArea = texture2DShader.GetUniform("rectangleBlendArea");
			texture2DClipArea = texture2DShader.GetUniform("rectangleClipArea");

			texture2DArrayProjectionMatrix = texture2DArrayShader.GetUniform("pMatrix");
			texture2DArrayOffset = texture2DArrayShader.GetUniform("rectangleOffset");
			texture2DArrayScale = texture2DArrayShader.GetUniform("rectangleScale");
			texture2DArrayTexture = texture2DArrayShader.GetUniform("rectangleTexture");
			texture2DArrayBlendArea = texture2DArrayShader.GetUniform("rectangleBlendArea");
			texture2DArrayClipArea = texture2DArrayShader.GetUniform("rectangleClipArea");
			texture2DArrayDepth = texture2DArrayShader.GetUniform("textureDepth");

		}

	}

}