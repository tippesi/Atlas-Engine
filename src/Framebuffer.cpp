#include "Framebuffer.h"

namespace Atlas {

	uint32_t Framebuffer::boundFramebufferID = 0;

	Framebuffer::Framebuffer() {

		glGenFramebuffers(1, &ID);

		drawBuffersSet = false;

	}

	Framebuffer::Framebuffer(const Framebuffer& that) {

		glGenFramebuffers(1, &ID);

		drawBuffersSet = false;

	}

	Framebuffer::Framebuffer(int32_t width, int32_t height) : width(width), height(height) {

		glGenFramebuffers(1, &ID);

		drawBuffersSet = false;

	}

	Framebuffer::~Framebuffer() {

		for (auto &componentKey : components) {
			if (componentKey.second.internalTexture) {
				delete componentKey.second.texture;
			}
		}

		glDeleteFramebuffers(1, &ID);

	}

	Framebuffer& Framebuffer::operator=(Framebuffer &that) {

		if (this != &that) {

			this->width = that.width;
			this->height = that.height;

		}

		return *this;

	}

	void Framebuffer::AddComponent(int32_t attachment, int32_t sizedFormat, int32_t wrapping,
								   int32_t filtering, uint32_t target) {

		FramebufferComponent component;

		auto search = components.find(attachment);

		if (search != components.end()) {
			component = search->second;
			// Check if the component texture was created internally
			if (component.internalTexture)
				delete component.texture;
		}

		component.texture = new Texture::Texture2D(width, height, sizedFormat, wrapping, filtering, false, false);
		component.textureArray = nullptr;
		component.cubemap = nullptr;
		component.internalTexture = true;
		component.index = 0;
		component.target = target;

		Bind();

		glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, component.texture->GetID(), 0);

		components[attachment] = component;

		if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 &&
			search == components.end() && !drawBuffersSet) {
			drawBuffers.push_back(attachment);
			glDrawBuffers((GLsizei) drawBuffers.size(), &drawBuffers[0]);
		}

	}

	void Framebuffer::AddComponentTexture(int32_t attachment, Texture::Texture2D *texture, uint32_t target) {

		FramebufferComponent component;

		auto search = components.find(attachment);

		if (search != components.end()) {
			component = search->second;
			// Check if the component texture was created internally
			if (component.internalTexture)
				delete component.texture;
		}

		component.texture = texture;
		component.textureArray = nullptr;
		component.cubemap = nullptr;
		component.internalTexture = false;
		component.index = 0;
		component.target = target;

		Bind();

		glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, component.texture->GetID(), 0);

		components[attachment] = component;

		if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 &&
			search == components.end() && !drawBuffersSet) {
			drawBuffers.push_back(attachment);
			glDrawBuffers((GLsizei) drawBuffers.size(), &drawBuffers[0]);
		}

	}

	void
	Framebuffer::AddComponentTextureArray(int32_t attachment, Texture::Texture2DArray *texture, int32_t layer, uint32_t target) {

		FramebufferComponent component;

		auto search = components.find(attachment);

		if (search != components.end()) {
			component = search->second;
			// Check if the component texture was created internally
			if (component.internalTexture)
				delete component.texture;
		}

		component.texture = nullptr;
		component.textureArray = texture;
		component.cubemap = nullptr;
		component.internalTexture = false;
		component.index = layer;
		component.target = target;

		Bind();

		glFramebufferTextureLayer(target, attachment, texture->GetID(), 0, layer);

		components[attachment] = component;

		if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 &&
			search == components.end() && !drawBuffersSet) {
			drawBuffers.push_back(attachment);
			glDrawBuffers((GLsizei) drawBuffers.size(), &drawBuffers[0]);
		}

	}

	void Framebuffer::AddComponentCubemap(int32_t attachment, Texture::Cubemap *cubemap, int32_t face, uint32_t target) {

		FramebufferComponent component;

		auto search = components.find(attachment);

		if (search != components.end()) {
			component = search->second;
			// Check if the component texture was created internally
			if (component.internalTexture)
				delete component.texture;
		}

		component.texture = nullptr;
		component.textureArray = nullptr;
		component.cubemap = cubemap;
		component.internalTexture = false;
		component.index = 0;
		component.target = target;

		Bind();

		glFramebufferTexture2D(target, attachment, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubemap->GetID(), 0);

		components[attachment] = component;

		if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 &&
			search == components.end() && !drawBuffersSet) {
			drawBuffers.push_back(attachment);
			glDrawBuffers((GLsizei) drawBuffers.size(), &drawBuffers[0]);
		}

	}

	Texture::Texture2D *Framebuffer::GetComponentTexture(int32_t attachment) {

		auto search = components.find(attachment);

		if (search != components.end()) {
			return search->second.texture;
		}

		return nullptr;

	}

	Texture::Texture2DArray *Framebuffer::GetComponentTextureArray(int32_t attachment) {

		auto search = components.find(attachment);

		if (search != components.end()) {
			return search->second.textureArray;
		}

		return nullptr;

	}

	Texture::Cubemap *Framebuffer::GetComponentCubemap(int32_t attachment) {

		auto search = components.find(attachment);

		if (search != components.end()) {
			return search->second.cubemap;
		}

		return nullptr;

	}

	void Framebuffer::Resize(int32_t width, int32_t height) {

		Bind();

		this->width = width;
		this->height = height;

		for (auto &componentKey : components) {
			FramebufferComponent &component = componentKey.second;

			if (component.texture != nullptr) {
				component.texture->Resize(width, height);
				glFramebufferTexture2D(component.target, componentKey.first,
									   GL_TEXTURE_2D, component.texture->GetID(), 0);
			}

			if (component.textureArray != nullptr) {
				component.textureArray->Resize(width, height, component.textureArray->layers);
				glFramebufferTextureLayer(component.target, componentKey.first,
										  component.textureArray->GetID(), 0, component.index);
			}

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

	void Framebuffer::SetDrawBuffers(std::vector<uint32_t> drawBuffers) {

		Bind();

		glDrawBuffers((int32_t)drawBuffers.size(), drawBuffers.data());

		drawBuffersSet = true;

	}

}