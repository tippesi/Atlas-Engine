#include "Framebuffer.h"

uint32_t Framebuffer::boundFramebufferID = 0;

Framebuffer::Framebuffer(int32_t width, int32_t height) : width(width), height(height) {

	glGenFramebuffers(1, &ID);

	drawBuffersSet = false;

}

void Framebuffer::AddComponent(int32_t attachment, GLenum dataFormat, int32_t internalFormat, int32_t wrapping, int32_t filtering) {

	FramebufferComponent* component = nullptr;

	auto search = components.find(attachment);

	if (search == components.end()) {
		component = new FramebufferComponent;
	}
	else {
		component = search->second;
		// Check if the component texture was created internally
		if (component->internalTexture)
			delete component->texture;
	}

	component->texture = new Texture(dataFormat, width, height, internalFormat, 0.0f, wrapping, filtering, false, false);
	component->internalTexture = true;

	Bind();

	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, component->texture->GetID(), 0);

	components[attachment] = component;

	if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 && 
		search == components.end() && !drawBuffersSet) {
		drawBuffers.push_back(attachment);
		glDrawBuffers((GLsizei)drawBuffers.size(), &drawBuffers[0]);
	}

}

void Framebuffer::AddComponent(int32_t attachment, Texture* texture) {

	FramebufferComponent* component = nullptr;

	auto search = components.find(attachment);

	if (search == components.end()) {
		component = new FramebufferComponent;
	}
	else {
		component = search->second;
		// Check if the component texture was created internally
		if (component->internalTexture)
			delete component->texture;
	}

	component->texture = texture;
	component->internalTexture = false;

	Bind();

	glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D, component->texture->GetID(), 0);

	components[attachment] = component;

	if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 &&
		search == components.end() && !drawBuffersSet) {
		drawBuffers.push_back(attachment);
		glDrawBuffers((GLsizei)drawBuffers.size(), &drawBuffers[0]);
	}

}

void Framebuffer::AddComponentLayer(int32_t attachment, Texture* texture, int32_t layer) {

	FramebufferComponent* component = nullptr;

	auto search = components.find(attachment);

	if (search == components.end()) {
		component = new FramebufferComponent;
	}
	else {
		component = search->second;
		// Check if the component texture was created internally
		if (component->internalTexture)
			delete component->texture;
	}

	component->texture = texture;
	component->internalTexture = false;

	Bind();

	glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment, texture->GetID(), 0, layer);

	components[attachment] = component;

	if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 &&
		search == components.end() && !drawBuffersSet) {
		drawBuffers.push_back(attachment);
		glDrawBuffers((GLsizei)drawBuffers.size(), &drawBuffers[0]);
	}

}

Texture* Framebuffer::GetComponent(int32_t attachment) {

	auto search = components.find(attachment);

	if (search == components.end()) {
		return nullptr;
	}
	else {
		return search->second->texture;
	}

}

void Framebuffer::Resize(int32_t width, int32_t height) {

	for (auto& componentKey : components) {
		FramebufferComponent* component = componentKey.second;
		component->texture->Resize(width, height);
	}

}

void Framebuffer::Bind(bool resizeViewport) {

	if (boundFramebufferID != ID) {

		if (resizeViewport) {
			glViewport(0, 0, width, height);
		}

		glBindFramebuffer(GL_FRAMEBUFFER, ID);
	
		boundFramebufferID = ID;
	}
}

void Framebuffer::Unbind() {

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	boundFramebufferID = 0;

}

void Framebuffer::SetDrawBuffers(uint32_t* drawBuffers, int32_t count) {

	Bind();

	glDrawBuffers(count, drawBuffers);

	drawBuffersSet = true;

}

void Framebuffer::ClearContent() {

	components.clear();
	drawBuffers.clear();

	drawBuffersSet = false;

}

void Framebuffer::DeleteContent() {

	for (auto& componentKey : components) {
		FramebufferComponent* component = componentKey.second;
		if (component->internalTexture) {
			delete component->texture;
		}
		delete component;
	}

	ClearContent();

}

Framebuffer::~Framebuffer() {

	glDeleteFramebuffers(1, &ID);

}