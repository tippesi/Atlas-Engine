#ifndef TEXTURE_H
#define TEXTURE_H

#include "system.h"

class Texture {

public:
	Texture(void* data, GLenum dataFormat, int32_t width, int32_t height, int32_t internalFormat,
		int32_t format, float LoD, int32_t wrapping, int32_t filtering, bool anisotropic, bool mipmaps);

	Texture(const char* filename, bool withoutCorrection = false);

	void MirrorHorizontally();

	~Texture();

	int32_t width;
	int32_t height;

	int32_t format;
	int32_t channels;

	uint8_t* data;

private:
	void UncorrectGamma();



	int32_t ID;

};

#endif