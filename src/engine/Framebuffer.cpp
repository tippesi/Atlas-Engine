#include "Framebuffer.h"

namespace Atlas {

	Framebuffer::Framebuffer() {

		// glGenFramebuffers(1, &ID);

		drawBuffersSet = false;

	}

	Framebuffer::Framebuffer(const Framebuffer& that) {

		// glGenFramebuffers(1, &ID);

		drawBuffersSet = false;

	}

	Framebuffer::Framebuffer(int32_t width, int32_t height) : width(width), height(height) {

		// glGenFramebuffers(1, &ID);

		drawBuffersSet = false;

	}

	Framebuffer::~Framebuffer() {

		for (auto &componentKey : components) {
			if (componentKey.second.internalTexture) {
				delete componentKey.second.texture;
			}
		}

		// glDeleteFramebuffers(1, &ID);

	}

	/*
	TODO
	Framebuffer& Framebuffer::operator=(Framebuffer &that) {

		if (this != &that) {

			this->width = that.width;
			this->height = that.height;

		}

		return *this;

	}
	*/

	void Framebuffer::AddComponent(int32_t attachment, int32_t sizedFormat, int32_t wrapping,
								   int32_t filtering, uint32_t target) {

		auto component = GetComponent(attachment);

		component.texture = new Texture::Texture2D(width, height, sizedFormat, wrapping, filtering, false, false);
		component.internalTexture = true;
		component.target = target;

		Bind();

        // glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, component.texture->GetID(), 0);

		UpdateDrawBuffers(attachment);
		components[attachment] = component;		

	}

	void Framebuffer::AddComponentTexture(int32_t attachment, Texture::Texture2D *texture, uint32_t target) {

		auto component = GetComponent(attachment);

		component.texture = texture;
		component.target = target;

		Bind();

        // glFramebufferTexture2D(target, attachment, GL_TEXTURE_2D, component.texture->GetID(), 0);

		UpdateDrawBuffers(attachment);
		components[attachment] = component;

	}

	void Framebuffer::AddComponentTextureArray(int32_t attachment, Texture::Texture2DArray *texture, int32_t layer, uint32_t target) {

		auto component = GetComponent(attachment);

		component.textureArray = texture;
		component.index = layer;
		component.target = target;

		Bind();

        // glFramebufferTextureLayer(target, attachment, texture->GetID(), 0, layer);

		UpdateDrawBuffers(attachment);
		components[attachment] = component;

	}

	void Framebuffer::AddComponentCubemap(int32_t attachment, Texture::Cubemap *cubemap, int32_t face, uint32_t target) {

		auto component = GetComponent(attachment);

		component.cubemap = cubemap;
		component.target = target;

		Bind();

        // glFramebufferTexture2D(target, attachment, GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, cubemap->GetID(), 0);

		UpdateDrawBuffers(attachment);
		components[attachment] = component;

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
			auto &component = componentKey.second;

			if (component.texture) {
				component.texture->Resize(width, height);
                // glFramebufferTexture2D(component.target, componentKey.first,
                // 					   GL_TEXTURE_2D, component.texture->GetID(), 0);
			}

			if (component.textureArray) {
				component.textureArray->Resize(width, height, component.textureArray->depth);
                // glFramebufferTextureLayer(component.target, componentKey.first,
                // 						  component.textureArray->GetID(), 0, component.index);
			}

		}

	}

	void Framebuffer::Bind(bool resizeViewport) {

		if (resizeViewport) {
            // glViewport(0, 0, width, height);
		}

        // glBindFramebuffer(GL_FRAMEBUFFER, ID);
	}

	void Framebuffer::Bind(int32_t target, bool resizeViewport) {

		if (resizeViewport) {
            // glViewport(0, 0, width, height);
		}

        // glBindFramebuffer(target, ID);
	}

	void Framebuffer::Unbind() {

        // glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}

	void Framebuffer::SetDrawBuffers(std::vector<uint32_t> drawBuffers) {

		Bind();

        // glDrawBuffers((int32_t)drawBuffers.size(), drawBuffers.data());

		drawBuffersSet = true;

	}

	Framebuffer::Component Framebuffer::GetComponent(int32_t attachment) {

		Component component;

		auto search = components.find(attachment);

		if (search != components.end()) {
			component = search->second;
			// Check if the component texture was created internally
			if (component.internalTexture)
				delete component.texture;
		}

		return component;

	}

	void Framebuffer::UpdateDrawBuffers(int32_t attachment) {

		auto search = components.find(attachment);

        /*
		if (attachment >= GL_COLOR_ATTACHMENT0 && attachment <= GL_COLOR_ATTACHMENT15 &&
			search == components.end() && !drawBuffersSet) {
			drawBuffers.push_back(attachment);
			glDrawBuffers((GLsizei)drawBuffers.size(), &drawBuffers[0]);
		}
         */

	}

}