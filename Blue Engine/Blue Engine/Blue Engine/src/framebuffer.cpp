#include "framebuffer.h"

uint32_t Framebuffer::boundFramebufferID = 0;

Framebuffer::Framebuffer(int32_t width, int32_t height) : width(width), height(height) {

	glGenFramebuffers(1, &ID);

}

void Framebuffer::AddComponent(int32_t attachment, GLenum dataFormat, int32_t internalFormat, int32_t wrapping, int32_t filtering) {

	Texture* texture = new Texture(dataFormat, width, height, internalFormat, 0.0f, wrapping, filtering, false, false);

	Bind();

	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture->GetID(), 0);

	components.push_back(texture);

	if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15) {
		drawBuffers.push_back(attachment);
		glDrawBuffers((GLsizei)drawBuffers.size(), &drawBuffers[0]);
	}

}

void Framebuffer::Resize(int32_t width, int32_t height) {

	for (Texture* texture : components) {
		texture->Resize(width, height);
	}

}

void Framebuffer::Bind() {

	if (boundFramebufferID != ID) {

		glBindFramebuffer(GL_FRAMEBUFFER, ID);
	
		boundFramebufferID = ID;
	}
}

void Framebuffer::Unbind() {

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	boundFramebufferID = 0;

}

void Framebuffer::SetDrawBuffers(uint32_t* drawBuffers, int32_t count) {

	glNamedFramebufferDrawBuffers(ID, count, drawBuffers);

}

Framebuffer::~Framebuffer() {



}