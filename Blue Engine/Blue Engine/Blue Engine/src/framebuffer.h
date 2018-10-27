#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "system.h"
#include "texture.h"
#include <list>

class Framebuffer {

public:
	Framebuffer(int32_t width, int32_t height);

	void AddComponent(int32_t attachment, GLenum dataFormat, int32_t internalFormat, int32_t wrapping, int32_t filtering);

	void Resize(int32_t width, int32_t height);

	void Bind();

	void UnBind();

	int32_t width;
	int32_t height;

	list<Texture*> components;

private:
	uint32_t ID;

	static uint32_t boundFramebufferID;

};

#endif