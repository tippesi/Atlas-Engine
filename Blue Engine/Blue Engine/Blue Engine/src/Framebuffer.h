#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "System.h"
#include "Texture.h"
#include <vector>

class Framebuffer {

public:
	Framebuffer(int32_t width, int32_t height);

	void AddComponent(int32_t attachment, GLenum dataFormat, int32_t internalFormat, int32_t wrapping, int32_t filtering);

	void AddComponent(int32_t attachment, Texture* texture);

	void Resize(int32_t width, int32_t height);

	void Bind();

	void Unbind();

	void SetDrawBuffers(uint32_t* buffers, int32_t count);

	~Framebuffer();

	int32_t width;
	int32_t height;

	vector<Texture*> components;
	vector<uint32_t> drawBuffers;

private:
	uint32_t ID;

	bool drawBuffersSet;

	static uint32_t boundFramebufferID;

};

#endif