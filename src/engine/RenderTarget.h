#ifndef AE_RENDERTARGET_H
#define AE_RENDERTARGET_H

#include "System.h"
#include "texture/Texture2D.h"
#include "graphics/RenderPass.h"
#include "graphics/Framebuffer.h"

namespace Atlas {

	enum RenderResolution {
		FULL_RES = 0,
		HALF_RES
	};

	class RenderTargetData {
	public:
		RenderTargetData() = default;

		explicit RenderTargetData(ivec2 resolution) {
            baseColorTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
			depthTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_D32_SFLOAT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest);
			normalTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
			geometryNormalTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16B16A16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
			roughnessMetallicAoTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R8G8B8A8_UNORM, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
			offsetTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R8_SINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
			materialIdxTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R16_UINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest);
            stencilTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y, VK_FORMAT_R8_UINT,
                Texture::Wrapping::ClampToEdge, Texture::Filtering::Nearest);
            velocityTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
            swapVelocityTexture = std::make_shared<Texture::Texture2D>(resolution.x, resolution.y,
                VK_FORMAT_R16G16_SFLOAT, Texture::Wrapping::ClampToEdge, Texture::Filtering::Linear);
        }

		void Resize(ivec2 resolution) {
			depthTexture->Resize(resolution.x, resolution.y);
			normalTexture->Resize(resolution.x, resolution.y);
			geometryNormalTexture->Resize(resolution.x, resolution.y);
			roughnessMetallicAoTexture->Resize(resolution.x, resolution.y);
			offsetTexture->Resize(resolution.x, resolution.y);
			materialIdxTexture->Resize(resolution.x, resolution.y);
            stencilTexture->Resize(resolution.x, resolution.y);
            velocityTexture->Resize(resolution.x, resolution.y);
            swapVelocityTexture->Resize(resolution.x, resolution.y);
		}

		Ref<Texture::Texture2D> baseColorTexture = nullptr;
		Ref<Texture::Texture2D> depthTexture = nullptr;
        Ref<Texture::Texture2D> normalTexture = nullptr;
        Ref<Texture::Texture2D> geometryNormalTexture = nullptr;
        Ref<Texture::Texture2D> roughnessMetallicAoTexture = nullptr;
        Ref<Texture::Texture2D> offsetTexture = nullptr;
        Ref<Texture::Texture2D> materialIdxTexture = nullptr;
        Ref<Texture::Texture2D> stencilTexture = nullptr;
        Ref<Texture::Texture2D> velocityTexture = nullptr;
        Ref<Texture::Texture2D> swapVelocityTexture = nullptr;
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

		RenderTargetData* GetData(RenderResolution resolution);

		RenderTargetData* GetHistoryData(RenderResolution resolution);

		Texture::Texture2D* GetHistory();

		Texture::Texture2D* GetLastHistory();

		Texture::Texture2D* GetVelocity();

		Texture::Texture2D* GetLastVelocity();

        Ref<Graphics::RenderPass> gBufferRenderPass;
        Ref<Graphics::FrameBuffer> gBufferFrameBuffer;

        Ref<Graphics::RenderPass> lightingRenderPass;
        Ref<Graphics::FrameBuffer> lightingFrameBuffer;
        Ref<Graphics::FrameBuffer> lightingFrameBufferWithStencil;

		Texture::Texture2D postProcessTexture;

		Texture::Texture2D aoTexture;
		Texture::Texture2D swapAoTexture;
		Texture::Texture2D historyAoTexture;
		Texture::Texture2D aoMomentsTexture;
		Texture::Texture2D historyAoMomentsTexture;

		Texture::Texture2D volumetricTexture;
		Texture::Texture2D swapVolumetricTexture;

		Texture::Texture2D volumetricCloudsTexture;
		Texture::Texture2D swapVolumetricCloudsTexture;
		Texture::Texture2D historyVolumetricCloudsTexture;

		Texture::Texture2D reflectionTexture;
		Texture::Texture2D swapReflectionTexture;
		Texture::Texture2D historyReflectionTexture;
		Texture::Texture2D reflectionMomentsTexture;
		Texture::Texture2D historyReflectionMomentsTexture;

        Texture::Texture2D lightingTexture;
        Texture::Texture2D hdrTexture;

	private:
		Texture::Texture2D historyTexture;
		Texture::Texture2D swapHistoryTexture;

        RenderTargetData targetData;
		RenderTargetData targetDataDownsampled2x;
		RenderTargetData targetDataSwapDownsampled2x;

		int32_t width = 0;
		int32_t height = 0;

		RenderResolution aoResolution;
		RenderResolution volumetricResolution;
		RenderResolution reflectionResolution;

		bool swap = false;

	};

}


#endif