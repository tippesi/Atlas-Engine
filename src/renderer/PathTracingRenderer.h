#ifndef AE_GPURAYTRACINGRENDERER_H
#define AE_GPURAYTRACINGRENDERER_H

#include "../System.h"
#include "Renderer.h"
#include "helper/RayTracingHelper.h"

#include "../texture/TextureAtlas.h"

#include <unordered_map>

namespace Atlas {

	namespace Renderer {

		class PathTracerRenderTarget {

		public:
			PathTracerRenderTarget() {}

			PathTracerRenderTarget(int32_t width, int32_t height) : width(width), height(height) {
				texture = Texture::Texture2D(width, height, AE_RGBA8,
					GL_CLAMP_TO_EDGE, GL_LINEAR);

				accumTexture0 = Texture::Texture2D(width, height, AE_RGBA32F);
				accumTexture1 = Texture::Texture2D(width, height, AE_RGBA32F);

			}

			void Resize(int32_t width, int32_t height) {
				this->width = width;
				this->height = height;

				texture.Resize(width, height);

				accumTexture0.Resize(width, height);
				accumTexture1.Resize(width, height);
			}

			int32_t GetWidth() const { return width; }
			int32_t GetHeight() const { return height; }

			Texture::Texture2D texture;

			Texture::Texture2D accumTexture0;
			Texture::Texture2D accumTexture1;

			int32_t sampleCount = 0;

		private:
			int32_t width = 0;
			int32_t height = 0;

		};

		class PathTracingRenderer : public Renderer {

		public:
			PathTracingRenderer();

			void Render(Viewport* viewport, RenderTarget* target, Camera* camera, Scene::Scene* scene) final;

			void Render(Viewport* viewport, PathTracerRenderTarget* renderTarget,
				ivec2 imageSubdivisions, Camera* camera, Scene::Scene* scene);

			bool UpdateData(Scene::Scene* scene);

			void UpdateMaterials(Scene::Scene* scene);

			void ResetSampleCount();

			int32_t GetSampleCount() const;

			int32_t bounces = 10;
			int32_t bvhDepth = 0;
			int32_t lightCount = 512;

		private:
			void GetRayGenUniforms();
			void GetRayHitUniforms();

			Helper::RayTracingHelper helper;

			vec3 cameraLocation;
			vec2 cameraRotation;

			int32_t sampleCount = 0;
			ivec2 imageOffset = ivec2(0);

			Shader::Shader rayGenShader;

			Shader::Uniform* cameraLocationRayGenUniform = nullptr;
			Shader::Uniform* originRayGenUniform = nullptr;
			Shader::Uniform* rightRayGenUniform = nullptr;
			Shader::Uniform* bottomRayGenUniform = nullptr;

			Shader::Uniform* sampleCountRayGenUniform = nullptr;
			Shader::Uniform* pixelOffsetRayGenUniform = nullptr;

			Shader::Uniform* tileSizeRayGenUniform = nullptr;
			Shader::Uniform* resolutionRayGenUniform = nullptr;

			Shader::Shader rayHitShader;

			Shader::Uniform* maxBouncesRayHitUniform = nullptr;

			Shader::Uniform* sampleCountRayHitUniform = nullptr;
			Shader::Uniform* bounceCountRayHitUniform = nullptr;

			Shader::Uniform* resolutionRayHitUniform = nullptr;

			Shader::Uniform* seedRayHitUniform = nullptr;

		};

	}

}

#endif