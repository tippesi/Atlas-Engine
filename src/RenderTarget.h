#ifndef AE_RENDERTARGET_H
#define AE_RENDERTARGET_H

#include "System.h"
#include "Framebuffer.h"

namespace Atlas {

	enum RenderResolution {
		FULL_RES = 0,
		HALF_RES
	};

	class DownsampledRenderTarget {
	public:
		DownsampledRenderTarget() = default;

		DownsampledRenderTarget(ivec2 resolution) {
			depthTexture = new Texture::Texture2D(resolution.x, resolution.y, AE_R32F, GL_CLAMP_TO_EDGE, GL_NEAREST);
			normalTexture = new Texture::Texture2D(resolution.x, resolution.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
			geometryNormalTexture = new Texture::Texture2D(resolution.x, resolution.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
			roughnessMetallicAoTexture = new Texture::Texture2D(resolution.x, resolution.y, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR);
			velocityTexture = new Texture::Texture2D(resolution.x, resolution.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_NEAREST);
			offsetTexture = new Texture::Texture2D(resolution.x, resolution.y, AE_R8I, GL_CLAMP_TO_EDGE, GL_LINEAR);
			materialIdxTexture = new Texture::Texture2D(resolution.x, resolution.y, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST);
		}

		void Resize(ivec2 resolution) {
			depthTexture->Resize(resolution.x, resolution.y);
			normalTexture->Resize(resolution.x, resolution.y);
			geometryNormalTexture->Resize(resolution.x, resolution.y);
			roughnessMetallicAoTexture->Resize(resolution.x, resolution.y);
			velocityTexture->Resize(resolution.x, resolution.y);
			offsetTexture->Resize(resolution.x, resolution.y);
			materialIdxTexture->Resize(resolution.x, resolution.y);
		}

		Texture::Texture2D* depthTexture = nullptr;
		Texture::Texture2D* normalTexture = nullptr;
		Texture::Texture2D* geometryNormalTexture = nullptr;
		Texture::Texture2D* roughnessMetallicAoTexture = nullptr;
		Texture::Texture2D* velocityTexture = nullptr;
		Texture::Texture2D* offsetTexture = nullptr;
		Texture::Texture2D* materialIdxTexture = nullptr;
	};

	class RenderTarget {

	public:
		/**
		 * Constructs a RenderTarget object.
		 */
		RenderTarget() = default;

		/**
		 * Constructs a RenderTarget object.
		 * @param width The width of the render target
		 * @param height The height of the render target
		 */
		explicit RenderTarget(int32_t width, int32_t height);

		/**
		 * The render target doesn't support copy due to framebuffers
		 */
		RenderTarget& operator=(const RenderTarget& that) = delete;

		/**
		 * Resizes the render target.
		 * @param width The width of the render target
		 * @param height The height of the render target
		 */
		void Resize(int32_t width, int32_t height);

		/**
		 * Returns the width of the render target.
		 * @return The width of the render target.
		 */
		int32_t GetWidth();

		/**
		 * Returns the width of the render target.
		 * @return The width of the render target.
		 */
		int32_t GetHeight();

		/**
		 * Swaps the history textures.
		 * @note This is used internally for TAA.
		 */
		void Swap();

		/**
		 * Returns the relative resolution based on the render target resolution
		 * @param resolution The resolution "factor" for the relative resolution
		 */
		ivec2 GetRelativeResolution(RenderResolution resolution);

		/*
		 * Sets the render resolution for screen space ambient occlusion
		 */
		void SetAOResolution(RenderResolution resolution);

		/*
		 * Gets the render resolution for screen space ambient occlusion
		 */
		RenderResolution GetAOResolution();

		/*
		 * Sets the render resolution for volumetric effetcs
		 */
		void SetVolumetricResolution(RenderResolution resolution);

		/*
		 * Gets the render resolution for volumetric effetcs
		 */
		RenderResolution GetVolumetricResolution();

		/*
		 * Sets the render resolution for reflections
		 */
		void SetReflectionResolution(RenderResolution resolution);

		/*
		 * Gets the render resolution for reflections
		 */
		RenderResolution GetReflectionResolution();

		DownsampledRenderTarget* GetDownsampledTextures(RenderResolution resolution);

		DownsampledRenderTarget* GetDownsampledHistoryTextures(RenderResolution resolution);

		Texture::Texture2D* GetHistory();

		Texture::Texture2D* GetLastHistory();

		Texture::Texture2D* GetVelocity();

		Texture::Texture2D* GetLastVelocity();

		Framebuffer geometryFramebuffer;
		Framebuffer lightingFramebuffer;
		Framebuffer postProcessFramebuffer;

		Texture::Texture2D postProcessTexture;

		Texture::Texture2D aoTexture;
		Texture::Texture2D swapAoTexture;
		Texture::Texture2D historyAoTexture;
		Texture::Texture2D aoMomentsTexture;
		Texture::Texture2D historyAoMomentsTexture;

		Texture::Texture2D volumetricTexture;
		Texture::Texture2D swapVolumetricTexture;

		Texture::Texture2D reflectionTexture;
		Texture::Texture2D swapReflectionTexture;
		Texture::Texture2D historyReflectionTexture;
		Texture::Texture2D reflectionMomentsTexture;
		Texture::Texture2D historyReflectionMomentsTexture;

	private:
		Texture::Texture2D depthTexture;
		Texture::Texture2D normalTexture;
		Texture::Texture2D stencilTexture;

		Texture::Texture2D velocityTexture;
		Texture::Texture2D swapVelocityTexture;

		Texture::Texture2D historyTexture;
		Texture::Texture2D swapHistoryTexture;

		DownsampledRenderTarget downsampledTarget1x;
		DownsampledRenderTarget downsampledTarget2x;
		DownsampledRenderTarget downsampledSwapTarget2x;

		int32_t width = 0;
		int32_t height = 0;

		RenderResolution aoResolution;
		RenderResolution volumetricResolution;
		RenderResolution reflectionResolution;

		bool swap = false;

	};

}


#endif