#include "RenderTarget.h"

namespace Atlas {

	RenderTarget::RenderTarget(int32_t width, int32_t height) : width(width), height(height) {

		// We want a shared depth and velocity texture across the geometry and lighting framebuffers
		depthTexture = Texture::Texture2D(width, height, AE_DEPTH32F,
			GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);
		normalTexture = Texture::Texture2D(width, height, AE_RGB16F,
			GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

		velocityTexture = Texture::Texture2D(width, height, AE_RG16F,
			GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
		swapVelocityTexture = Texture::Texture2D(width, height, AE_RG16F,
			GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

		stencilTexture = Texture::Texture2D(width, height, AE_R8UI,
			GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);

		geometryFramebuffer.Resize(width, height);

		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT1, AE_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT3, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT4, AE_R16UI, GL_CLAMP_TO_EDGE, GL_NEAREST);
		geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT2, &normalTexture);
		geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT5, &velocityTexture);
		geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT6, &stencilTexture);
		geometryFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);

		lightingFramebuffer.Resize(width, height);

		lightingFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		lightingFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT1, &velocityTexture);
		lightingFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT2, &stencilTexture);
		lightingFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);

		postProcessFramebuffer.Resize(width, height);

		postProcessFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);

		historyTexture = Texture::Texture2D(width, height, AE_RGBA16F,
			GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
		swapHistoryTexture = Texture::Texture2D(width, height, AE_RGBA16F,
				GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

		postProcessTexture = Texture::Texture2D(width, height, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR);

		SetAOResolution(HALF_RES);
		SetVolumetricResolution(HALF_RES);
		SetReflectionResolution(HALF_RES);

		ivec2 halfRes = GetRelativeResolution(HALF_RES);
		downsampledTarget2x = DownsampledRenderTarget(halfRes);
		downsampledSwapTarget2x = DownsampledRenderTarget(halfRes);

		sssTexture = Texture::Texture2D(width, height, AE_RGBA32F, GL_CLAMP_TO_EDGE, GL_LINEAR);

	}

	void RenderTarget::Resize(int32_t width, int32_t height) {

		geometryFramebuffer.Resize(width, height);
		lightingFramebuffer.Resize(width, height);

		geometryFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);
		geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT5, &velocityTexture);
		geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT6, &stencilTexture);

		postProcessFramebuffer.Resize(width, height);

		// We have to also resize the other part of the history
		historyTexture.Resize(width, height);
		velocityTexture.Resize(width, height);

		swapHistoryTexture.Resize(width, height);
		swapVelocityTexture.Resize(width, height);

		postProcessTexture.Resize(width, height);

		this->width = width;
		this->height = height;

		SetAOResolution(aoResolution);
		SetVolumetricResolution(volumetricResolution);
		SetReflectionResolution(reflectionResolution);

		ivec2 halfRes = GetRelativeResolution(HALF_RES);
		downsampledTarget2x.Resize(halfRes);
		downsampledSwapTarget2x.Resize(halfRes);

		sssTexture.Resize(width, height);

	}

	int32_t RenderTarget::GetWidth() {

		return width;

	}

	int32_t RenderTarget::GetHeight() {

		return height;

	}

	void RenderTarget::Swap() {

		if (swap) {
			geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT5, &velocityTexture);
			lightingFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT1, &velocityTexture);
		}
		else {
			geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT5, &swapVelocityTexture);
			lightingFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT1, &swapVelocityTexture);
		}

		swap = !swap;

	}

	ivec2 RenderTarget::GetRelativeResolution(RenderResolution resolution) {

		int32_t factor = 1;

		switch (resolution) {
		case HALF_RES: factor = 2; break;
		default: break;
		}

		return ivec2(width / factor, height / factor);

	}

	void RenderTarget::SetAOResolution(RenderResolution resolution) {

		auto res = GetRelativeResolution(resolution);
		aoResolution = resolution;

		aoTexture = Texture::Texture2D(res.x, res.y, AE_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		swapAoTexture = Texture::Texture2D(res.x, res.y, AE_R16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

		aoMomentsTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		historyAoMomentsTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

	}

	RenderResolution RenderTarget::GetAOResolution() {

		return aoResolution;

	}

	void RenderTarget::SetVolumetricResolution(RenderResolution resolution) {

		auto res = GetRelativeResolution(resolution);
		volumetricResolution = resolution;

		volumetricTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F);
		swapVolumetricTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F);

		volumetricCloudsTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		swapVolumetricCloudsTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		historyVolumetricCloudsTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

	}

	RenderResolution RenderTarget::GetVolumetricResolution() {

		return volumetricResolution;

	}

	void RenderTarget::SetReflectionResolution(RenderResolution resolution) {

		auto res = GetRelativeResolution(resolution);
		reflectionResolution = resolution;

		reflectionTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		swapReflectionTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);

		reflectionMomentsTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		historyReflectionMomentsTexture = Texture::Texture2D(res.x, res.y, AE_RGBA16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
	
	}

	RenderResolution RenderTarget::GetReflectionResolution() {

		return reflectionResolution;

	}

	DownsampledRenderTarget* RenderTarget::GetDownsampledTextures(RenderResolution resolution) {

		switch (resolution) {
		case FULL_RES:
			downsampledTarget1x.depthTexture = &depthTexture;
			downsampledTarget1x.normalTexture = geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT1);
			downsampledTarget1x.geometryNormalTexture = &normalTexture;
			downsampledTarget1x.velocityTexture = GetLastVelocity();
			downsampledTarget1x.roughnessMetallicAoTexture = geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT3);
			downsampledTarget1x.materialIdxTexture = geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT4);
			return &downsampledTarget1x;
		default: return swap ? &downsampledTarget2x : &downsampledSwapTarget2x;
		}

	}

	DownsampledRenderTarget* RenderTarget::GetDownsampledHistoryTextures(RenderResolution resolution) {

		switch (resolution) {
		case FULL_RES:
			downsampledTarget1x.depthTexture = &depthTexture;
			downsampledTarget1x.normalTexture = geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT1);
			downsampledTarget1x.geometryNormalTexture = &normalTexture;
			downsampledTarget1x.velocityTexture = GetLastVelocity();
			downsampledTarget1x.roughnessMetallicAoTexture = geometryFramebuffer.GetComponentTexture(GL_COLOR_ATTACHMENT3);
			return &downsampledTarget1x;
		default: return swap ? &downsampledSwapTarget2x : &downsampledTarget2x;
		}

	}

	Texture::Texture2D* RenderTarget::GetHistory() {

		if (swap) {
			return &historyTexture;
		}
		else {
			return &swapHistoryTexture;
		}

	}

	Texture::Texture2D* RenderTarget::GetLastHistory() {

		if (swap) {
			return &swapHistoryTexture;
		}
		else {
			return &historyTexture;
		}

	}

	Texture::Texture2D* RenderTarget::GetVelocity() {

		if (swap) {
			return &velocityTexture;
		}
		else {
			return &swapVelocityTexture;
		}

	}

	Texture::Texture2D* RenderTarget::GetLastVelocity() {

		if (swap) {
			return &swapVelocityTexture;
		}
		else {
			return &velocityTexture;
		}

	}

}