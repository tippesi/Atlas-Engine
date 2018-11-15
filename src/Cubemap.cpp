#include "Cubemap.h"
#include "Texture.h"

// Declared in Texture.cpp
#include "libraries/stb/stb_image.h"

Cubemap::Cubemap(const char* right, const char* left, const char* top,
	const char* bottom, const char* front, const char* back) {

	const char* filenames[] = { right, left, top, bottom, front, back };

	int32_t width = 0, height = 0, channels = 0;

	glGenTextures(1, &ID);

	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

#ifdef ENGINE_SHOW_LOG
	EngineLog("Loading cubemap with ID %d", ID);
#endif

	for (uint32_t i = 0; i < 6; i++) {

		uint8_t* data = stbi_load(filenames[i], &width, &height, &channels, 3);

		if (data != nullptr) {
			// OpenGL ES doesn't guarantee that mipmaps are working in sRGB color space so we better ignore gamma correction
#ifdef ENGINE_OGL
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB8, width, height, 0,
				GL_RGB, GL_UNSIGNED_BYTE, data);
#else
			Texture::UncorrectGamma(data, width, height, channels, 2.2f);

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB8, width, height, 0,
				GL_RGB, GL_UNSIGNED_BYTE, data);
#endif
#ifdef ENGINE_SHOW_LOG
			EngineLog("    Loaded cubemap face %d %s", i, filenames[i]);
#endif
			delete data;
		}
		else {
#ifdef ENGINE_SHOW_LOG
			EngineLog("    Failed to load cubemap face %d %s", i, filenames[i]);
#endif
			throw EngineException("Failed to load cubemap");
		}

	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

}

void Cubemap::Bind(uint32_t unit) {

	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

}