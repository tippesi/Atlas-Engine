#include "texture.h"

//STB image library is declared(only once)
#define STB_IMAGE_IMPLEMENTATION
#include "libraries/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libraries/stb/stb_image_write.h"

// Static members have to be defined in the .cpp again
int32_t Texture::anisotropyLevel = 0;


Texture::Texture(GLenum dataFormat, int32_t width, int32_t height, 
	int32_t internalFormat,	float LoD, int32_t wrapping, int32_t filtering,
	bool anisotropic, bool mipmaps) : width(width), height(height) {

	int32_t format = GetBaseFormat(internalFormat);

	GenerateTexture(dataFormat, internalFormat, format, LoD, wrapping, filtering, anisotropic, mipmaps);

	data = new uint8_t[width * height * channels];

}


Texture::Texture(const char* filename, bool withoutCorrection) {

	width = 0;
	height = 0;
	channels = 0;

	float LoD = -0.4f;

	data = stbi_load(filename, &width, &height, &channels, 0);

	if (data == NULL) {
		throw new EngineException("Texture couldn't be loaded");
	}

	// OpenGL ES doesn't guarantee that mipmaps are working in sRGB color space so we better ignore gamma correction
#ifdef ENGINE_OGL
	if (channels == 4) {
		int32_t internalFormat = withoutCorrection ? GL_RGBA8 : GL_SRGB8_ALPHA8;
		GenerateTexture(GL_UNSIGNED_BYTE, internalFormat, GL_RGBA, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 3) {
		int32_t internalFormat = withoutCorrection ? GL_RGB8 : GL_SRGB8;
		GenerateTexture(GL_UNSIGNED_BYTE, internalFormat, GL_RGB, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 1) {
		GenerateTexture(GL_UNSIGNED_BYTE, GL_R8, GL_RED, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
#else
	// We want to uncorrect the data ourself, when OpenGL isn't doing it for us
	UncorrectGamma();

	// For OpenGL ES we use different texture formats
	if (channels == 4) {
		GenerateTexture(GL_UNSIGNED_BYTE, GL_RGBA4, GL_RGBA, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 3) {
		GenerateTexture(GL_UNSIGNED_BYTE, GL_RGB565, GL_RGB, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 1) {
		GenerateTexture(GL_UNSIGNED_BYTE, GL_R8, GL_RED, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
#endif

}

void Texture::Bind(uint32_t unit) {

	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_2D, ID);

}

void Texture::SetData(uint8_t* data) {

	delete this->data;

	this->data = data;

	glBindTexture(GL_TEXTURE_2D, ID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GetBaseFormat(internalFormat), dataFormat, data);

	if (mipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);

}

uint8_t* Texture::GetData() {

#ifdef ENGINE_OGL
	glBindTexture(GL_TEXTURE_2D, ID);
	glGetTexImage(GL_TEXTURE_2D, 0, GetBaseFormat(internalFormat), GL_UNSIGNED_BYTE, this->data);
#endif

	// We want to return a copy of the data
	uint8_t *data = new uint8_t[width * height* channels];
	memcpy(data, this->data, width * height * channels);

	return data;

}

void Texture::Resize(int32_t width, int32_t height) {

	this->width = width;
	this->height = height;

	delete data;
	data = new uint8_t[width * height * channels];

	glBindTexture(GL_TEXTURE_2D, ID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GetBaseFormat(internalFormat), dataFormat, NULL);

	if (mipmaps)
		glGenerateMipmap(GL_TEXTURE_2D);

}

void Texture::FlipHorizontally() {

	uint8_t* data = GetData();
	uint8_t* mirroredData = FlipDataHorizontally(data);
	SetData(mirroredData);

}

void Texture::SaveToPNG(const char* filename) {

	uint8_t* data = GetData();
	uint8_t* mirroredData = FlipDataHorizontally(data);

	stbi_write_png(filename, width, height, channels, mirroredData, width * channels);

	delete data;
	delete mirroredData;

}

Texture::~Texture() {



}

uint32_t Texture::GetID() {

	return ID;

}

int32_t Texture::GetMaxAnisotropyLevel() {

	const char* extensions = (const char*)glGetString(GL_EXTENSIONS);

	if (strstr(extensions, "GL_EXT_texture_filter_anisotropic")) {

		float maxAnisotropy;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

		if (!anisotropyLevel)
			anisotropyLevel = (int)maxAnisotropy;
		else
			anisotropyLevel = glm::min(anisotropyLevel, (int)maxAnisotropy);

		return (int32_t)maxAnisotropy;
	}
	else {
		return 0;
	}

}

void Texture::SetAnisotropyLevel(int32_t anisotropyLevel) {

	if (!Texture::anisotropyLevel)
		GetMaxAnisotropyLevel();

	if (Texture::anisotropyLevel) {
		float maxAnisotropy;
		glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

		Texture::anisotropyLevel = glm::min(anisotropyLevel, (int32_t)maxAnisotropy);
	}

}

void Texture::UncorrectGamma() {

	for (int32_t i = 0; i < width * height * channels; i++) {
		// Don't correct the aplha values
		if (channels == 4 && (i + 1) % 4 == 0)
			continue;
		// Before we can uncorrect it we have to bring it in normalized space
		data[i] = (uint8_t)(pow((float)data[i] / 255.0f, 2.2f) * 255.0f);
	}

}

uint8_t* Texture::FlipDataHorizontally(uint8_t* data) {

	if (data != NULL) {

		uint8_t* invertedData = new uint8_t[width * height * channels];

		if (invertedData != NULL) {

			int32_t dataIndex = width * (height + 1) * channels;

			for (int32_t i = 0; i < width * height * channels; i++) {

				if (dataIndex % (width * channels) == 0) {
					dataIndex = dataIndex - 2 * width * channels;
				}

				invertedData[dataIndex] = data[i];

				dataIndex++;

			}

			return invertedData;

		}

	}

	return NULL;

}

void Texture::GenerateTexture(GLenum dataFormat, int32_t internalFormat,
	int32_t format, float LoD, int32_t wrapping, int32_t filtering, bool anisotropic, bool mipmaps) {

	if (ID == 0)
		glGenTextures(1, &ID);

	glBindTexture(GL_TEXTURE_2D, ID);
	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, dataFormat, data);

	if (mipmaps) {

		glGenerateMipmap(GL_TEXTURE_2D);

		if (anisotropic) {

			if (anisotropyLevel) {

				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropyLevel);

			}
			else {

#ifdef ENGINE_OGL
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, LoD);
#endif

			}

		}
		else {

#ifdef ENGINE_OGL
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, LoD);
#endif

		}

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	}
	else {

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filtering);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filtering);

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrapping);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrapping);

	}

	glBindTexture(GL_TEXTURE_2D, 0);

	switch (format) {

	case GL_RGBA:
		channels = 4;
		break;
	case GL_RG:
		channels = 2;
		break;
	case  GL_RED:
		channels = 1;
		break;
	case  GL_R8:
		channels = 1;
		break;
	case  GL_DEPTH_COMPONENT:
		channels = 1;
		break;
	case  GL_DEPTH_STENCIL:
		channels = 1;
		break;
	default:
		channels = 3;
		break;

	}

	this->dataFormat = dataFormat;
	this->internalFormat = internalFormat;
	this->mipmaps = mipmaps;

}

int32_t Texture::GetBaseFormat(int32_t internalFormat) {

	// See https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glTexImage2D.xhtml
	// Might need a change for OpenGL ES
	switch (internalFormat) {
	case GL_R8: return GL_RED;
	case GL_R8_SNORM: return GL_RED;
	case GL_R16: return GL_RED;
	case GL_R16_SNORM: return GL_RED;
	case GL_RG8: return GL_RG;
	case GL_RG8_SNORM: return GL_RG;
	case GL_RG16: return GL_RG;
	case GL_RG16_SNORM: return GL_RG;
	case GL_RGB5_A1: return GL_RGBA;
	case GL_RGBA8: return GL_RGBA;
	case GL_RGBA8_SNORM: return GL_RGBA;
	case GL_RGB10_A2: return GL_RGBA;
	case GL_RGB10_A2UI: return GL_RGBA;
	case GL_RGBA12: return GL_RGBA;
	case GL_RGBA16: return GL_RGBA;
	case GL_SRGB8_ALPHA8: return GL_RGBA;
	case GL_R16F: return GL_RED;
	case GL_RG16F: return GL_RG;
	case GL_RGBA16F: return GL_RGBA;
	case GL_R32F: return GL_RED;
	case GL_RG32F: return GL_RG;
	case GL_RGBA32F: return GL_RGBA;
	case GL_DEPTH_COMPONENT16: return GL_DEPTH_COMPONENT;
	case GL_DEPTH_COMPONENT24: return GL_DEPTH_COMPONENT;
	case GL_DEPTH_COMPONENT32F: return GL_DEPTH_COMPONENT;
	case GL_DEPTH24_STENCIL8: return GL_DEPTH_STENCIL;
	case GL_DEPTH32F_STENCIL8: return GL_DEPTH_STENCIL;
	default: return GL_RGB;
	}

}