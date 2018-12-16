#include "Texture.h"
#include "Framebuffer.h"

//STB image library is declared(only once)
#define STB_IMAGE_IMPLEMENTATION
#include "libraries/stb/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "libraries/stb/stb_image_write.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "libraries/stb/stb_image_resize.h"

// Static members have to be defined in the .cpp again
int32_t Texture::anisotropyLevel = 0;

Texture::Texture(GLenum dataFormat, int32_t width, int32_t height, int32_t internalFormat, float LoD, int32_t wrapping,
	int32_t filtering, 	bool anisotropic, bool mipmaps, int32_t layerCount) : width(width), height(height), layerCount(layerCount) {

	ID = 0;

	int32_t format = GetBaseFormat(internalFormat);

	GenerateTexture(dataFormat, internalFormat, format, LoD, wrapping, filtering, anisotropic, mipmaps);

}

Texture::Texture(GLenum dataFormat, int32_t width, int32_t height, int32_t internalFormat, float LoD, int32_t wrapping,
	int32_t filtering, bool anisotropic, bool mipmaps) : width(width), height(height) {

	ID = 0;
	layerCount = 0;

	int32_t format = GetBaseFormat(internalFormat);

	GenerateTexture(dataFormat, internalFormat, format, LoD, wrapping, filtering, anisotropic, mipmaps);

}

Texture::Texture(string filename, bool withoutCorrection) {

	ID = 0;
	width = 0;
	height = 0;
	channels = 0;
	layerCount = 0;

	float LoD = -0.4f;

	uint8_t* data = stbi_load(filename.c_str(), &width, &height, &channels, 0);

	auto dataVector = vector<uint8_t>();

	dataVector.assign(data, data + width * height * channels);

	delete[] data;

	if (data == nullptr) {
#ifdef ENGINE_SHOW_LOG
		EngineLog("Failed to load texture %s", filename.c_str());
#endif
		throw EngineException("Texture couldn't be loaded");
	}

	// OpenGL ES doesn't guarantee that mipmaps are working in sRGB color space so we better ignore gamma correction
#ifdef ENGINE_GL
	if (channels == 4) {
		if (!withoutCorrection)
			GammaToLinear(dataVector.data(), width, height, channels);
		GenerateTexture(GL_UNSIGNED_BYTE, GL_RGBA8, GL_RGBA, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 3) {
		if (!withoutCorrection)
			GammaToLinear(dataVector.data(), width, height, channels);
		GenerateTexture(GL_UNSIGNED_BYTE, GL_RGB8, GL_RGB, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 1) {
		GenerateTexture(GL_UNSIGNED_BYTE, GL_R8, GL_RED, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
#elif ENGINE_GLES
	// For OpenGL ES we use different texture formats
	if (channels == 4) {
		if (!withoutCorrection)
			GammaToLinear(dataVector.data(), width, height, channels);
		GenerateTexture(GL_UNSIGNED_BYTE, GL_RGBA8, GL_RGBA, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 3) {
		if (!withoutCorrection)
			GammaToLinear(dataVector.data(), width, height, channels);
		GenerateTexture(GL_UNSIGNED_BYTE, GL_RGB8, GL_RGB, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
	if (channels == 1) {
		GenerateTexture(GL_UNSIGNED_BYTE, GL_R8, GL_RED, LoD, GL_CLAMP_TO_EDGE, 0, true, true);
	}
#endif

	SetData(dataVector);

#ifdef ENGINE_SHOW_LOG
	EngineLog("Loaded texture %s", filename.c_str());
#endif

}

void Texture::Bind(uint32_t unit) {

	glActiveTexture(unit);
	if (layerCount == 0) {
		glBindTexture(GL_TEXTURE_2D, ID);
	}
	else {
		glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
	}

}

void Texture::SetData(vector<uint8_t> data, int32_t layer, int32_t layerCount) {

	if (this->layerCount == 0) {
		glBindTexture(GL_TEXTURE_2D, ID);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GetBaseFormat(internalFormat), dataFormat, data.data());

		if (mipmaps)
			glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, width, height, layerCount, GetBaseFormat(internalFormat), dataFormat, data.data());
		if (mipmaps)
			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	}

}

vector<uint8_t> Texture::GetData(int32_t layer) {

	Framebuffer framebuffer = Framebuffer(width, height);

	vector<uint8_t> data = vector<uint8_t>(width * height * channels);

	if (layerCount == 0) {
		framebuffer.AddComponentTexture(GL_COLOR_ATTACHMENT0, this);
	}
	else {
		framebuffer.AddComponentTextureLayer(GL_COLOR_ATTACHMENT0, this, layer);
	}

 	glReadPixels(0, 0, width, height, GetBaseFormat(internalFormat), GL_UNSIGNED_BYTE, data.data());

	framebuffer.DeleteContent();

	framebuffer.Unbind();

	return data;

}

void Texture::Resize(int32_t width, int32_t height) {

	this->width = width;
	this->height = height;

	if (layerCount == 0) {
		glBindTexture(GL_TEXTURE_2D, ID);
		glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, GetBaseFormat(internalFormat), dataFormat, nullptr);
		if (mipmaps)
			glGenerateMipmap(GL_TEXTURE_2D);
	}
	else {
		glBindTexture(GL_TEXTURE_2D_ARRAY, ID);
		glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, internalFormat, width, height, layerCount, 0, GetBaseFormat(internalFormat)
			, dataFormat, NULL);
		if (mipmaps)
			glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
	}

}

void Texture::FlipHorizontally() {

	auto data = GetData();
	auto mirroredData = FlipDataHorizontally(data);
	SetData(mirroredData);

}

void Texture::SaveToPNG(string filename) {

	auto data = GetData();
	auto mirroredData = FlipDataHorizontally(data);

	stbi_write_png(filename.c_str(), width, height, channels, data.data(), width * channels);

}

uint32_t Texture::GetID() {

	return ID;

}

uint32_t Texture::GetDataFormat() {

	return dataFormat;

}

int32_t Texture::GetSizedDataFormat() {

	return internalFormat;

}

Texture::~Texture() {

	glDeleteTextures(1, &ID);

}

int32_t Texture::GetMaxAnisotropyLevel() {

	int32_t extensionCount = 0;
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

	for (int32_t i = 0; i < extensionCount; i++) {
		const char* extension = (const char*)glGetStringi(GL_EXTENSIONS, i);
		if (strcmp(extension, "GL_EXT_texture_filter_anisotropic")) {

			float maxAnisotropy;
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);

			if (!anisotropyLevel)
				anisotropyLevel = (int)maxAnisotropy;
			else
				anisotropyLevel = glm::min(anisotropyLevel, (int)maxAnisotropy);

			return (int32_t)maxAnisotropy;
		}
	}

	return 0;

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

void Texture::GammaToLinear(uint8_t* data, int32_t width, int32_t height, int32_t channels) {

	for (int32_t i = 0; i < width * height * channels; i++) {
		// Don't correct the aplha values
		if (channels == 4 && (i + 1) % 4 == 0)
			continue;

		// Using OpenGL conversion:
		float value = (float)data[i] / 255.0f;
		value = value <= 0.04045f ? value / 12.92f : powf((value + 0.055f) / 1.055f, 2.4f);
		// Before we can uncorrect it we have to bring it in normalized space
		data[i] = (uint8_t)(glm::clamp(value, 0.0f, 1.0f) * 255.0f);

	}

}

vector<uint8_t> Texture::FlipDataHorizontally(vector<uint8_t> data) {

	auto invertedData = vector<uint8_t>(width * height * channels);

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

void Texture::GenerateTexture(GLenum dataFormat, int32_t internalFormat,
	int32_t format, float LoD, int32_t wrapping, int32_t filtering, bool anisotropic, bool mipmaps) {

	if (ID == 0)
		glGenTextures(1, &ID);

	int32_t target = layerCount == 0 ? GL_TEXTURE_2D : GL_TEXTURE_2D_ARRAY;

	glBindTexture(target, ID);

	int32_t mipCount = mipmaps ? (int32_t)floor(log2(glm::max((float)width, (float)height))) + 1 : 1;

	if (layerCount == 0) {
		glTexStorage2D(GL_TEXTURE_2D, mipCount, internalFormat, width, height);
	}
	else {
		glTexStorage3D(GL_TEXTURE_2D_ARRAY, mipCount, internalFormat, width, height, layerCount);
	}

	if (mipmaps) {

		glGenerateMipmap(target);

		if (anisotropic) {

			if (anisotropyLevel) {

				glTexParameteri(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropyLevel);

			}
			else {

#ifdef ENGINE_GL
				glTexParameterf(target, GL_TEXTURE_LOD_BIAS, LoD);
#endif

			}

		}
		else {

#ifdef ENGINE_GL
			glTexParameterf(target, GL_TEXTURE_LOD_BIAS, LoD);
#endif

		}

		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	}
	else {

		glTexParameteri(target, GL_TEXTURE_MIN_FILTER, filtering);
		glTexParameteri(target, GL_TEXTURE_MAG_FILTER, filtering);

		glTexParameteri(target, GL_TEXTURE_WRAP_S, wrapping);
		glTexParameteri(target, GL_TEXTURE_WRAP_T, wrapping);

	}

	glBindTexture(target, 0);

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