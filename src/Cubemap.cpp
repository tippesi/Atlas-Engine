#include "Cubemap.h"
#include "texture/Texture.h"
#include "loader/ImageLoader.h"

Cubemap::Cubemap(string right, string left, string top,
	string bottom, string front, string back) {

	string filenames[] = { right, left, top, bottom, front, back };

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

		auto image = ImageLoader::LoadImage(filenames[i], false, 3);

		if (image.data.size() != 0) {
#ifdef ENGINE_GL
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_SRGB8, image.width, image.height, 0,
				GL_RGB, GL_UNSIGNED_BYTE, image.data.data());
#elif ENGINE_GLES
			Texture::GammaToLinear(image.data.data(), image.width, image.height, 3);

			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB8, image.width, image.height, 0,
				GL_RGB, GL_UNSIGNED_BYTE, image.data.data());
#endif
#ifdef ENGINE_SHOW_LOG
			EngineLog("    Loaded cubemap face %d %s", i, filenames[i].c_str());
#endif
		}
		else {
#ifdef ENGINE_SHOW_LOG
			EngineLog("    Failed to load cubemap face %d %s", i, filenames[i].c_str());
#endif
			throw EngineException("Failed to load cubemap");
		}

	}

	glGenerateMipmap(GL_TEXTURE_CUBE_MAP);

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

}

Cubemap::Cubemap(GLenum dataFormant, int32_t width, int32_t height, int32_t internalFormat,
		int32_t wrapping, int32_t filtering, bool mipmaps) {

	glGenTextures(1, &ID);

	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

	if (mipmaps) {
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	}
	else {
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, filtering);
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, filtering);
	}

	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, wrapping);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, wrapping);
	glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, wrapping);

	for (uint32_t i = 0; i < 6; i++) {

		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internalFormat, width, height, 0,
			Texture::GetBaseFormat(internalFormat), dataFormant, NULL);

	}

	if (mipmaps) {
		glGenerateMipmap(GL_TEXTURE_CUBE_MAP);
	}

	glBindTexture(GL_TEXTURE_CUBE_MAP, 0);


}

Cubemap::~Cubemap() {

	glDeleteTextures(1, &ID);

}

void Cubemap::Bind(uint32_t unit) {

	glActiveTexture(unit);
	glBindTexture(GL_TEXTURE_CUBE_MAP, ID);

}

uint32_t Cubemap::GetID() {

	return ID;

}