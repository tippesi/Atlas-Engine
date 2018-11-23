#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "System.h"
#include "Texture.h"
#include <unordered_map>
#include <vector>

class Framebuffer {

public:
	Framebuffer(int32_t width, int32_t height);

	void AddComponent(int32_t attachment, GLenum dataFormat, int32_t internalFormat, int32_t wrapping, int32_t filtering);

	void AddComponent(int32_t attachment, Texture* texture);

	void AddComponentLayer(int32_t attachment, Texture* texture, int32_t layer);

	Texture* GetComponent(int32_t attachment);

	void Resize(int32_t width, int32_t height);

	void Bind(bool resizeViewport = false);

	void Unbind();

	void SetDrawBuffers(uint32_t* buffers, int32_t count);

	~Framebuffer();

	int32_t width;
	int32_t height;

private:
	struct FramebufferComponent {
		Texture* texture;
		bool internalTexture;
	};

	uint32_t ID;

	bool drawBuffersSet;

	unordered_map<int32_t, FramebufferComponent*> components;
	vector<uint32_t> drawBuffers;

	static uint32_t boundFramebufferID;

};

#endif