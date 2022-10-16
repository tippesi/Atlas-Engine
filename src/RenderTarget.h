#ifndef AE_RENDERTARGET_H
#define AE_RENDERTARGET_H

#include "System.h"
#include "Framebuffer.h"

namespace Atlas {

	enum RenderResolution {
		FULL_RES = 0,
		HALF_RES
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
		RenderTarget(int32_t width, int32_t height);

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

		Texture::Texture2D* GetDownsampledDepthTexture(RenderResolution resolution);

		Texture::Texture2D* GetDownsampledNormalTexture(RenderResolution resolution);

		Texture::Texture2D* GetDownsampledRoughnessMetalnessAoTexture(RenderResolution resolution);

		Texture::Texture2D* GetDownsampledOffsetTexture(RenderResolution resolution);

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

		Texture::Texture2D volumetricTexture;
		Texture::Texture2D swapVolumetricTexture;

		Texture::Texture2D reflectionTexture;
		Texture::Texture2D swapReflectionTexture;

	private:
		Texture::Texture2D depthTexture;
		Texture::Texture2D normalTexture;
		Texture::Texture2D stencilTexture;

		Texture::Texture2D depthDownsampled2xTexture;
		Texture::Texture2D normalDownsampled2xTexture;
		Texture::Texture2D roughnessMetalnessAoDownsampled2xTexture;
        Texture::Texture2D downsampleOffset2xTexture;

		Texture::Texture2D velocityTexture;
		Texture::Texture2D swapVelocityTexture;

		Texture::Texture2D historyTexture;
		Texture::Texture2D swapHistoryTexture;

		int32_t width = 0;
		int32_t height = 0;

		RenderResolution aoResolution;
		RenderResolution volumetricResolution;
		RenderResolution reflectionResolution;

		bool swap = false;

	};

}


#endif