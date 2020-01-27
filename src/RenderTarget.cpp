#include "RenderTarget.h"

namespace Atlas {

	RenderTarget::RenderTarget(int32_t width, int32_t height) : width(width), height(height) {

		// We want a shared depth and velocity texture across the geometry and lighting framebuffers
		depthTexture = Texture::Texture2D(width, height, AE_DEPTH32F,
			GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);

		velocityTexture = Texture::Texture2D(width, height, AE_RG16F,
			GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);

		geometryFramebuffer.Resize(width, height);

		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT1, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT2, AE_RG16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT3, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT4, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT5, &velocityTexture);
		geometryFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);

		lightingFramebuffer.Resize(width, height);

		lightingFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		lightingFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT1, &velocityTexture);
		lightingFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);

		postProcessFramebuffer.Resize(width, height);

		postProcessFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);

		historyFramebuffer.Resize(width, height);

		historyTexture = Texture::Texture2D(width, height, AE_RGB16F,
			GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);
		swapHistoryTexture = Texture::Texture2D(width, height, AE_RGB16F,
				GL_CLAMP_TO_EDGE, GL_LINEAR, false, false);

		historyFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &historyTexture);

		historyFramebuffer.Unbind();

		postProcessTexture = Texture::Texture2D(width, height, AE_RGBA8, GL_CLAMP_TO_EDGE, GL_LINEAR);

	}

	void RenderTarget::Resize(int32_t width, int32_t height) {

		geometryFramebuffer.Resize(width, height);
		lightingFramebuffer.Resize(width, height);

		geometryFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);
		geometryFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT5, &velocityTexture);

		postProcessFramebuffer.Resize(width, height);

		historyFramebuffer.Resize(width, height);

		historyFramebuffer.Unbind();

		postProcessTexture.Resize(width, height);

		this->width = width;
		this->height = height;

	}

	int32_t RenderTarget::GetWidth() {

		return width;

	}

	int32_t RenderTarget::GetHeight() {

		return height;

	}

	void RenderTarget::Swap() {

		if (swap) {
			historyFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &historyTexture);
		}
		else {
			historyFramebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, &swapHistoryTexture);
		}

		historyFramebuffer.Unbind();

		swap = !swap;

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

		return &velocityTexture;

	}

}