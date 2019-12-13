#ifndef AE_RENDERTARGET_H
#define AE_RENDERTARGET_H

#include "System.h"
#include "Framebuffer.h"

namespace Atlas {

	class RenderTarget {

	public:
		/**
		 * Constructs a RenderTarget object.
		 */
		RenderTarget() {}

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

		Texture::Texture2D* GetHistory();

		Texture::Texture2D* GetVelocity();

		Framebuffer geometryFramebuffer;
		Framebuffer lightingFramebuffer;
		Framebuffer historyFramebuffer;

	private:
		Texture::Texture2D depthTexture;
		Texture::Texture2D velocityTexture;

		Texture::Texture2D historyTexture;
		Texture::Texture2D swapHistoryTexture;

		int32_t width = 0;
		int32_t height = 0;

		bool swap = false;

	};

}


#endif