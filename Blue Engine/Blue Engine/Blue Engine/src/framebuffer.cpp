#include "framebuffer.h"

uint32_t Framebuffer::boundFramebufferID = 0;

Framebuffer::Framebuffer(int32_t width, int32_t height) : width(width), height(height) {

	glGenFramebuffers(1, &ID);

}

void Framebuffer::AddComponent(int32_t attachment, GLenum dataFormat, int32_t internalFormat, int32_t wrapping, int32_t filtering) {

	Texture* texture = new Texture(dataFormat, width, height, internalFormat, 0.0f, wrapping, filtering, false, false);

	Bind();

	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, texture->GetID(), 0);

}

void Framebuffer::Resize(int32_t width, int32_t height) {

	for (list<Texture*>::iterator iterator = components.begin(); iterator != components.end(); iterator++) {
		(*iterator)->Resize(width, height);
	}

}

void Framebuffer::Bind() {

	if (boundFramebufferID != ID) {
		glBindFramebuffer(GL_FRAMEBUFFER, ID);
	
		boundFramebufferID = ID;
	}
}

void Framebuffer::UnBind() {

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	boundFramebufferID = 0;

}