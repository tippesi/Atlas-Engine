#include "RenderTarget.h"

namespace Atlas {

	RenderTarget::RenderTarget(int32_t width, int32_t height) : width(width), height(height) {

		// We want a shared depth texture across the geometry and lighting framebuffers
		depthTexture = Texture::Texture2D(width, height, AE_DEPTH24, 
			GL_CLAMP_TO_EDGE, GL_NEAREST, false, false);

		geometryFramebuffer.Resize(width, height);

		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT1, AE_RGB8, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponent(GL_COLOR_ATTACHMENT2, AE_RG16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		geometryFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);

		lightingFramebuffer.Resize(width, height);

		lightingFramebuffer.AddComponent(GL_COLOR_ATTACHMENT0, AE_RGB16F, GL_CLAMP_TO_EDGE, GL_LINEAR);
		lightingFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT, &depthTexture);

		lightingFramebuffer.Unbind();

	}

	void RenderTarget::Resize(int32_t width, int32_t height) {

		geometryFramebuffer.Resize(width, height);
		lightingFramebuffer.Resize(width, height);

		geometryFramebuffer.AddComponentTexture(GL_DEPTH_ATTACHMENT,
			lightingFramebuffer.GetComponentTexture(GL_DEPTH_ATTACHMENT));

		geometryFramebuffer.Unbind();

		this->width = width;
		this->height = height;

	}

	int32_t RenderTarget::GetWidth() {

		return width;

	}

	int32_t RenderTarget::GetHeight() {

		return height;

	}

}